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
#include <linux/of.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-async.h>
#include "acamera_logger.h"
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "acamera_types.h"
#include "soc_iq.h"

#include "runtime_initialization_settings.h"
#include "acamera_get_calibration_otp.h"
#include "acamera_calibrations.h"

static int cali_name;
int otp_enable = 0;

static char* sensor = NULL;
module_param(sensor, charp, 0);

module_param(otp_enable, int, 0664);
MODULE_PARM_DESC(otp_enable, "Use OTP memory for calibrations");

module_param(cali_name, int, 0664);

#define ARGS_TO_PTR( arg ) ( (struct soc_iq_ioctl_args *)arg )

#define __GET_LUT_SIZE( lut ) ( lut->rows * lut->cols * lut->width )
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

static struct v4l2_subdev soc_iq;

static ACameraCalibrations g_luts_arr[FIRMWARE_CONTEXT_NUMBER];

struct IqConversion {
    uint32_t (*calibration_init)(uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c);
    uint32_t (*calibration_otp)(uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c);
    const char *sensor_name;
};

struct IqConversion IqConversionTable[] = {
    {CALIBRATION_SUBDEV_FUNCTIONS_OS08A10, CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_OTP, "os08a10"},
    {CALIBRATION_SUBDEV_FUNCTIONS_IMX290, NULL, "imx290"},
    {CALIBRATION_SUBDEV_FUNCTIONS_IMX335, NULL, "imx335"},
    {CALIBRATION_SUBDEV_FUNCTIONS_IMX290, NULL, "imx290sub"},
    {CALIBRATION_SUBDEV_FUNCTIONS_IMX290, NULL, "imx290ssub"},
    {CALIBRATION_SUBDEV_FUNCTIONS_IMX290, NULL, "imx290sssub"},
    {CALIBRATION_SUBDEV_FUNCTIONS_IMX415, NULL, "imx415"},
    {CALIBRATION_SUBDEV_FUNCTIONS_IMX227, CALIBRATION_SUBDEV_FUNCTIONS_IMX227_OTP, "imx227"},
    {CALIBRATION_SUBDEV_FUNCTIONS_IMX307, NULL, "imx307"},
    {CALIBRATION_SUBDEV_FUNCTIONS_OV5675, NULL, "ov5675"},
};

uint32_t ( *CALIBRATION_FUNC_ARR[] )( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c ) = {CALIBRATION_SUBDEV_FUNCTIONS_IMX290, CALIBRATION_SUBDEV_FUNCTIONS_IMX290, CALIBRATION_SUBDEV_FUNCTIONS_IMX290};
uint32_t ( *CALIBRATION_OTP_FUNC_ARR[] )( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c ) = {CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_OTP, CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_OTP, CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_OTP};

static int iq_log_status( struct v4l2_subdev *sd )
{
    LOG( LOG_INFO, "log status called" );
    return 0;
}

static int get_cali_name_id( int cali_name_id, int sensor_name_id )
{

    if (strcmp(IqConversionTable[sensor_name_id].sensor_name, "os08a10") == 0) {
        switch ( cali_name_id ) {
        case 0:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_IPC;
            LOG( LOG_CRIT, "Loading Calibration for OS08A10_IPC\n" );
            break;
        case 1:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_SLT;
            LOG( LOG_CRIT, "Loading Calibration for OS08A10_SLT\n" );
            break;
        case 2:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_TV;
            LOG( LOG_CRIT, "Loading Calibration for OS08A10_TV\n" );
            break;
        case 3:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_PANEL;
            LOG( LOG_CRIT, "Loading Calibration for OS08A10_PANEL\n" );
            break;
        default:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_IPC;
            LOG( LOG_CRIT, "Loading Calibration for OS08A10_IPC\n" );
            break;
        }
    }

    if (strcmp(IqConversionTable[sensor_name_id].sensor_name, "imx290sssub") == 0) {
        switch ( cali_name_id ) {
        case 0:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290;
            LOG( LOG_CRIT, "get_calibrations_imx290\n" );
            break;
        case 1:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_SLT;
            LOG( LOG_CRIT, "get_calibrations_imx290 slt\n" );
            break;
        case 2:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_8mm;
            LOG( LOG_CRIT, "get_calibrations_imx290_lens_8mm\n" );
            break;
        case 3:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_4mm;
            LOG( LOG_CRIT, "get_calibrations_imx290_lens_4mm\n" );
            break;
        default:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290;
            LOG( LOG_CRIT, "get_calibrations_imx290\n" );
            break;
        }
    }

    if (strcmp(IqConversionTable[sensor_name_id].sensor_name, "imx290ssub") == 0) {
        switch ( cali_name_id ) {
        case 0:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290;
            LOG( LOG_CRIT, "get_calibrations_imx290\n" );
            break;
        case 1:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_SLT;
            LOG( LOG_CRIT, "get_calibrations_imx290 slt\n" );
            break;
        case 2:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_8mm;
            LOG( LOG_CRIT, "get_calibrations_imx290_lens_8mm\n" );
            break;
        case 3:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_4mm;
            LOG( LOG_CRIT, "get_calibrations_imx290_lens_4mm\n" );
            break;
        default:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290;
            LOG( LOG_CRIT, "get_calibrations_imx290\n" );
            break;
        }
    }

    if (strcmp(IqConversionTable[sensor_name_id].sensor_name, "imx290sub") == 0) {
        switch ( cali_name_id ) {
        case 0:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290;
            LOG( LOG_CRIT, "get_calibrations_imx290\n" );
            break;
        case 1:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_SLT;
            LOG( LOG_CRIT, "get_calibrations_imx290 slt\n" );
            break;
        case 2:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_8mm;
            LOG( LOG_CRIT, "get_calibrations_imx290_lens_8mm\n" );
            break;
        case 3:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_4mm;
            LOG( LOG_CRIT, "get_calibrations_imx290_lens_4mm\n" );
            break;
        default:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290;
            LOG( LOG_CRIT, "get_calibrations_imx290\n" );
            break;
        }
    }

    if (strcmp(IqConversionTable[sensor_name_id].sensor_name, "imx290") == 0) {
        switch ( cali_name_id ) {
        case 0:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290;
            LOG( LOG_CRIT, "get_calibrations_imx290\n" );
            break;
        case 1:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_SLT;
            LOG( LOG_CRIT, "get_calibrations_imx290 slt\n" );
            break;
        case 2:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_8mm;
            LOG( LOG_CRIT, "get_calibrations_imx290_lens_8mm\n" );
            break;
        case 3:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_4mm;
            LOG( LOG_CRIT, "get_calibrations_imx290_lens_4mm\n" );
            break;
        default:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX290;
            LOG( LOG_CRIT, "get_calibrations_imx290\n" );
            break;
        }
    }

    if (strcmp(IqConversionTable[sensor_name_id].sensor_name, "imx307") == 0) {
        switch ( cali_name_id ) {
        case 0:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX307_DEMO;
            LOG( LOG_CRIT, "get_calibrations_imx307_demo\n" );
            break;
        case 1:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX307;
            LOG( LOG_CRIT, "get_calibrations_imx307\n" );
            break;
        default:
            CALIBRATION_FUNC_ARR[0] = CALIBRATION_SUBDEV_FUNCTIONS_IMX307_DEMO;
            LOG( LOG_CRIT, "get_calibrations_imx307_demo\n" );
            break;
        }
    }

    return 0;
}

static int iq_init( struct v4l2_subdev *sd, u32 val )
{
    int rc = 0;
    LOG( LOG_INFO, "IQ calibrations initialization" );
    return rc;
}


static long iq_ioctl( struct v4l2_subdev *sd, unsigned int cmd, void *arg )
{
    long rc = 0;
    int i = 0;
    switch ( cmd ) {
    case V4L2_SOC_IQ_IOCTL_REQUEST_INFO: {
        int32_t context = ARGS_TO_PTR( arg )->ioctl.request_info.context;
        void *sensor_arg = ARGS_TO_PTR( arg )->ioctl.request_info.sensor_arg;
        int32_t id = ARGS_TO_PTR( arg )->ioctl.request_info.id;
        if ( context < FIRMWARE_CONTEXT_NUMBER && id < CALIBRATION_TOTAL_SIZE ) {
            memset(&g_luts_arr[context], 0, sizeof(g_luts_arr[0]));
            CALIBRATION_FUNC_ARR[context]( context, sensor_arg, &g_luts_arr[context] );
            ACameraCalibrations *luts_ptr = &g_luts_arr[context];
            ARGS_TO_PTR( arg )
                ->ioctl.request_info.lut.ptr = NULL;
            if ( luts_ptr->calibrations[id] ) {
                *( (uint16_t *)&ARGS_TO_PTR( arg )->ioctl.request_info.lut.rows ) = luts_ptr->calibrations[id]->rows;
                *( (uint16_t *)&ARGS_TO_PTR( arg )->ioctl.request_info.lut.cols ) = luts_ptr->calibrations[id]->cols;
                *( (uint16_t *)&ARGS_TO_PTR( arg )->ioctl.request_info.lut.width ) = luts_ptr->calibrations[id]->width;
                LOG( LOG_DEBUG, "Requested info for context:%d sensor_arg:0x%x table:%d - returned rows %d, cols %d, width %d", context, sensor_arg, id, luts_ptr->calibrations[id]->rows, luts_ptr->calibrations[id]->cols, luts_ptr->calibrations[id]->width );
            } else {
                *( (uint16_t *)&ARGS_TO_PTR( arg )->ioctl.request_info.lut.rows ) = 0;
                *( (uint16_t *)&ARGS_TO_PTR( arg )->ioctl.request_info.lut.cols ) = 0;
                *( (uint16_t *)&ARGS_TO_PTR( arg )->ioctl.request_info.lut.width ) = 0;
                LOG( LOG_INFO, "Unitialialized calibration at ctx:%d id:%d\n", context, id );
            }
            rc = 0;
        } else {
            LOG( LOG_ERR, "Requested info for context: %d sensor_arg :0x%x  table :%d but the maximum lut id is %d", context, sensor_arg, id, CALIBRATION_TOTAL_SIZE - 1 );
            rc = -1;
        }
    } break;
    case V4L2_SOC_IQ_IOCTL_REQUEST_DATA: {
        int32_t context = ARGS_TO_PTR( arg )->ioctl.request_data.context;
        void *sensor_arg = ARGS_TO_PTR( arg )->ioctl.request_data.sensor_arg;
        int32_t id = ARGS_TO_PTR( arg )->ioctl.request_data.id;
        int32_t data_size = ARGS_TO_PTR( arg )->ioctl.request_data.data_size;
        void *ptr = ARGS_TO_PTR( arg )->ioctl.request_data.ptr;
        if ( context < FIRMWARE_CONTEXT_NUMBER && id < CALIBRATION_TOTAL_SIZE ) {
            CALIBRATION_FUNC_ARR[context]( context, sensor_arg, &g_luts_arr[context] );
            ACameraCalibrations *luts_ptr = &g_luts_arr[context];
            if ( ptr != NULL ) {
                if ( luts_ptr->calibrations[id] ) {
                    if ( data_size == __GET_LUT_SIZE( luts_ptr->calibrations[id] ) ) {
                        if ( ARGS_TO_PTR( arg )->ioctl.request_data.kernel == 0 ) {
                            if ( copy_to_user( ptr, luts_ptr->calibrations[id]->ptr, (uint32_t)__GET_LUT_SIZE( luts_ptr->calibrations[id] ) ) <= 0 )
                                LOG( LOG_CRIT, "copy_to_user failed\n" );
                        } else {
                            memcpy( ptr, luts_ptr->calibrations[id]->ptr, (uint32_t)__GET_LUT_SIZE( luts_ptr->calibrations[id] ) );
                        }
                        rc = 0;
                    } else {
                        LOG( LOG_ERR, "Input lut size is %d bytes but must be %d bytes", data_size, __GET_LUT_SIZE( luts_ptr->calibrations[id] ) );
                        rc = -1;
                    }
                } else {
                    LOG( LOG_INFO, "Uninitialized calibration at ctx:%d id:%d\n", context, id );
                }
            } else {
                LOG( LOG_ERR, "User pointer is null, lut id %d, lut sensor_arg 0x%x", id, sensor_arg );
                rc = -1;
            }
        } else {
            LOG( LOG_ERR, "Requested info for context: %d sensor_arg :0x%x  table :%d the maximum lut id is %d", context, sensor_arg, id, CALIBRATION_TOTAL_SIZE - 1 );
            rc = -1;
        }
    } break;
    case V4L2_SOC_IQ_IOCTL_SET_SENSOR_NAME: {
        int32_t context = ARGS_TO_PTR( arg )->ioctl.request_data.context;
        void *sensor_arg = ARGS_TO_PTR( arg )->ioctl.request_data.sensor_arg;
        if (sensor_arg) {
            for (i = 0; i < NELEM(IqConversionTable); ++i) {
                if (strcmp(IqConversionTable[i].sensor_name, sensor_arg) == 0) {
                    CALIBRATION_FUNC_ARR[context] = IqConversionTable[i].calibration_init;
                    CALIBRATION_OTP_FUNC_ARR[context] = IqConversionTable[i].calibration_otp;
                    get_cali_name_id( cali_name, i );
                    sensor = (char *)IqConversionTable[i].sensor_name;
                    LOG(LOG_CRIT, "soc_iq get config %s according to sensor drv", sensor_arg);
                    rc = 0;
                    break;
                }
            }
        } else {
            LOG(LOG_CRIT, "set sensor name null, use default");
        }
    } break;
    case V4L2_SOC_IQ_IOCTL_REQUEST_OTP: {
        int32_t context = ARGS_TO_PTR( arg )->ioctl.request_data.context;
        void *sensor_arg = ARGS_TO_PTR( arg )->ioctl.request_data.sensor_arg;
        if ( context < FIRMWARE_CONTEXT_NUMBER ) {
            if (CALIBRATION_OTP_FUNC_ARR[context])
                rc = CALIBRATION_OTP_FUNC_ARR[context]( context, sensor_arg, &g_luts_arr[context] );
        } else {
            LOG( LOG_ERR, "Request OTP Fail.");
            rc = -1;
        }
    } break;
    case V4L2_SOC_IQ_IOCTL_REQUEST_EXTERNAL_CALI: {
        int32_t context = ARGS_TO_PTR( arg )->ioctl.request_data.context;
        void *sensor_arg = ARGS_TO_PTR( arg )->ioctl.request_data.sensor_arg;
        if ( context < FIRMWARE_CONTEXT_NUMBER ) {
            if (CALIBRATION_FUNC_ARR[context]) {
                rc = CALIBRATION_FUNC_ARR[context]( context, sensor_arg, &g_luts_arr[context] );
                get_calibratin_external_tuning(sensor, sensor_arg, &g_luts_arr[context]);
            }
        } else {
            LOG( LOG_ERR, "Request OTP Fail.");
            rc = -1;
        }
    } break;
    default:
        LOG( LOG_WARNING, "Unknown soc iq ioctl cmd %d", cmd );
        rc = -1;
        break;
    };

    return rc;
}


static const struct v4l2_subdev_core_ops core_ops = {
    .log_status = iq_log_status,
    .init = iq_init,
    .ioctl = iq_ioctl,
};


static const struct v4l2_subdev_ops iq_ops = {
    .core = &core_ops,
};

static int32_t soc_iq_probe( struct platform_device *pdev )
{
    int32_t rc = 0;
    int rtn = 0;
    int i;
    const char *sensor_name;
    struct device *dev = &pdev->dev;

    uint32_t total_functions = sizeof( CALIBRATION_FUNC_ARR ) / sizeof( CALIBRATION_FUNC_ARR[0] );
    if ( total_functions < FIRMWARE_CONTEXT_NUMBER ) {
        LOG( LOG_CRIT, "calibration functions mismatch total_functions:%d expected:%d", total_functions, FIRMWARE_CONTEXT_NUMBER );
    }

    if (sensor == NULL) {
        rtn = of_property_read_string(dev->of_node, "sensor-name", &sensor_name);

        if (rtn != 0) {
            pr_err("%s: iq failed to parse dts sensor name\n", __func__);
            return -1;
        }
    } else {
        sensor_name = sensor;
    }

    pr_err("probe iq_dev for sensor: %s\n", sensor_name);
/*
    for (i = 0; i < NELEM(IqConversionTable); ++i) {
        if (strcmp(IqConversionTable[i].sensor_name, sensor_name) == 0) {
               CALIBRATION_FUNC_ARR[0] = IqConversionTable[i].calibration_init;
               get_cali_name_id( cali_name, i );
               sensor = (char *)IqConversionTable[i].sensor_name;
               break;
        }
    }

    if (i == NELEM(IqConversionTable)) {
        CALIBRATION_FUNC_ARR[0] = IqConversionTable[0].calibration_init;
        pr_err("Fatal error:cant find %s iq with dts config and use default!\n", sensor_name);
    }
*/
    v4l2_subdev_init( &soc_iq, &iq_ops );

    soc_iq.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

    snprintf( soc_iq.name, V4L2_SUBDEV_NAME_SIZE, "%s", V4L2_SOC_IQ_NAME );

    soc_iq.dev = &pdev->dev;
    rc = v4l2_async_register_subdev( &soc_iq );

    for (i = 0; i < CALIBRATION_TOTAL_SIZE; i++)
        (&g_luts_arr[0])->calibrations[i] = NULL;

    LOG( LOG_ERR, "register v4l2 IQ device. result %d, sd 0x%x sd->dev 0x%x", rc, &soc_iq, soc_iq.dev );

    return rc;
}

static int soc_iq_remove( struct platform_device *pdev )
{
    v4l2_async_unregister_subdev( &soc_iq );
    return 0;
}

static struct platform_device *soc_iq_dev;

static const struct of_device_id sensor_dt_match[] = {
    {.compatible = "soc, iq"},
    {}};

static struct platform_driver soc_iq_driver = {
    .probe = soc_iq_probe,
    .remove = soc_iq_remove,
    .driver = {
        .name = "soc_iq",
        .owner = THIS_MODULE,
        .of_match_table = sensor_dt_match,
    },
};

int __init acamera_iq_iq_init( void )
{
    LOG( LOG_INFO, "IQ subdevice init" );

    soc_iq_dev = platform_device_register_simple(
        "soc_iq_v4l2", -1, NULL, 0 );
    return platform_driver_register( &soc_iq_driver );
}


void __exit acamera_iq_iq_exit( void )
{
    LOG( LOG_INFO, "IQ subdevice exit" );

    platform_driver_unregister( &soc_iq_driver );
    platform_device_unregister( soc_iq_dev );
}


module_init( acamera_iq_iq_init );
module_exit( acamera_iq_iq_exit );
MODULE_LICENSE( "GPL v2" );
MODULE_AUTHOR( "ARM IVG AC" );
