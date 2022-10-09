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
#include <media/v4l2-device.h>
#include <media/v4l2-async.h>
#include <linux/clk.h>
#include "acamera_logger.h"
#include "acamera_sensor_api.h"
#include "soc_sensor.h"
#include "acamera_firmware_config.h"
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "runtime_initialization_settings.h"
#include "sensor_bsp_common.h"
#include <linux/pm_runtime.h>

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend early_suspend;
#endif

static int isp_seq_num;
static int sensor_idx;
static int cur_idx;
static int info_idx;
module_param(isp_seq_num, int, 0664);

static char* sensor = NULL;
module_param(sensor, charp, 0);

#define ARGS_TO_PTR( arg ) ( (struct soc_sensor_ioctl_args *)arg )

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

struct SensorConversion {
    void (*sensor_init)(void **ctx, sensor_control_t *ctrl, void* sbp);
    void (*sensor_deinit)(void *ctx);
    int (*sensor_detect)(void* sbp);
    const char *sensor_name;
    int w;// max width
    int h;// max height
    char *res;
};

struct SensorConversion ConversionTable[][10] = {
    {
        {SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290, SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290, SENSOR_DETECT_FUNCTIONS_IMX290, "imx290", 1920,1080, "2MP"},
        {SENSOR_INIT_SUBDEV_FUNCTIONS_IMX307, SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX307, SENSOR_DETECT_FUNCTIONS_IMX307, "imx307", 1920,1080, "2MP"},
        {SENSOR_INIT_SUBDEV_FUNCTIONS_IMX335, SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX335, SENSOR_DETECT_FUNCTIONS_IMX335, "imx335", 2592,1944, "5MP"},
        {SENSOR_INIT_SUBDEV_FUNCTIONS_OV5640, SENSOR_DEINIT_SUBDEV_FUNCTIONS_OV5640, SENSOR_DETECT_FUNCTIONS_OV5640, "ov5640", 2560,1944, "5MP"},
        {SENSOR_INIT_SUBDEV_FUNCTIONS_OV5675, SENSOR_DEINIT_SUBDEV_FUNCTIONS_OV5675, SENSOR_DETECT_FUNCTIONS_OV5675, "ov5675", 2592,1944, "5MP"},
        {SENSOR_INIT_SUBDEV_FUNCTIONS_OS08A10, SENSOR_DEINIT_SUBDEV_FUNCTIONS_OS08A10, SENSOR_DETECT_FUNCTIONS_OS08A10, "os08a10", 3840,2160, "8MP"},
        {SENSOR_INIT_SUBDEV_FUNCTIONS_LT6911, SENSOR_DEINIT_SUBDEV_FUNCTIONS_LT6911, SENSOR_DETECT_FUNCTIONS_LT6911, "lt6911", 1920,1080, "2MP"},
    },
#if FIRMWARE_CONTEXT_NUMBER >= 2
    {
        {SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SUB, SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SUB, SENSOR_DETECT_FUNCTIONS_IMX290SUB, "imx290sub", 1920,1080, "2MP"},
        {SENSOR_INIT_SUBDEV_FUNCTIONS_OV5640SUB, SENSOR_DEINIT_SUBDEV_FUNCTIONS_OV5640SUB, SENSOR_DETECT_FUNCTIONS_OV5640SUB, "ov5640sub", 2560,1944, "5MP"},
    },
#endif
#if FIRMWARE_CONTEXT_NUMBER >= 3
    {
        {SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SSUB, SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SSUB, SENSOR_DETECT_FUNCTIONS_IMX290SSUB, "imx290ssub", 1920,1080, "2MP"},
    },
#endif
#if FIRMWARE_CONTEXT_NUMBER == 4
    {
        {SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SSSUB, SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SSSUB, SENSOR_DETECT_FUNCTIONS_IMX290SSSUB, "imx290sssub", 1920,1080, "2MP"},
    }
#endif
};

void ( *SOC_SENSOR_SENSOR_ENTRY_ARR[FIRMWARE_CONTEXT_NUMBER] )( void **ctx, sensor_control_t *ctrl, void* sbp ) =
{
    SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290,
#if FIRMWARE_CONTEXT_NUMBER >= 2
    SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SUB,
#endif
#if FIRMWARE_CONTEXT_NUMBER >= 3
    SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SSUB,
#endif
#if FIRMWARE_CONTEXT_NUMBER == 4
    SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SSUB
#endif
};
void ( *SOC_SENSOR_SENSOR_RESET_ARR[FIRMWARE_CONTEXT_NUMBER] )( void *ctx ) =
{
    SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290,
#if FIRMWARE_CONTEXT_NUMBER >= 2
    SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SUB,
#endif
#if FIRMWARE_CONTEXT_NUMBER >= 3
    SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SSUB,
#endif
#if FIRMWARE_CONTEXT_NUMBER == 4
    SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SSSUB
#endif
};
int ( *SOC_SENSOR_SENSOR_DETECT_ARR[FIRMWARE_CONTEXT_NUMBER] )( void *ctx ) =
{
    SENSOR_DETECT_FUNCTIONS_IMX290,
#if FIRMWARE_CONTEXT_NUMBER >= 2
    SENSOR_DETECT_FUNCTIONS_IMX290SUB,
#endif
#if FIRMWARE_CONTEXT_NUMBER >= 3
    SENSOR_DETECT_FUNCTIONS_IMX290SSUB,
#endif
#if FIRMWARE_CONTEXT_NUMBER == 4
    SENSOR_DETECT_FUNCTIONS_IMX290SSSUB,
#endif
};

static struct v4l2_subdev soc_sensor;
sensor_bringup_t* sensor_bp = NULL;
const char *sensor_name[FIRMWARE_CONTEXT_NUMBER];

typedef struct _subdev_camera_ctx {
    void *camera_context;
    sensor_control_t camera_control;
    unsigned char *s_name;
} subdev_camera_ctx;

static subdev_camera_ctx s_ctx[FIRMWARE_CONTEXT_NUMBER];

static int camera_log_status( struct v4l2_subdev *sd )
{
    LOG( LOG_INFO, "log status called" );
    return 0;
}

uint32_t write_reg(uint32_t val, unsigned long addr)
{
    void __iomem *io_addr;

    io_addr = ioremap_nocache(addr, 8);
    if (io_addr == NULL) {
        LOG(LOG_ERR, "%s: Failed to ioremap addr\n", __func__);
        return -1;
    }
    __raw_writel(val, io_addr);
    iounmap(io_addr);
    return 0;
}

static void parse_param(char *buf_orig, char **parm){
    char *ps, *token;
    unsigned int n = 0;
    char delim1[3] = " \n";
    ps = buf_orig;
    token = strsep(&ps, delim1);
    while (token != NULL) {
        if (*token != '\0') {
            parm[n++] = token;
        }
        token = strsep(&ps, delim1);
    }
}

static const char *sensor_reg_usage_str = {
    "Usage:\n"
    "echo r addr(H) > /sys/devices/platform/sensor/sreg;\n"
    "echo w addr(H) value(H) > /sys/devices/platform/sensor/sreg;\n"
    "echo d addr(H) num(D) > /sys/devices/platform/sensor/sreg; dump reg from addr\n"
};

static ssize_t sreg_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
    return sprintf(buf, "%s\n", sensor_reg_usage_str);
}

static ssize_t sreg_write(
    struct device *dev, struct device_attribute *attr,
    char const *buf, size_t size)
{
    char *buf_orig = NULL;
    char *parm[8] = {NULL};
    long val = 0;
    unsigned int reg_addr, reg_val, i;
    ssize_t ret = size;

    subdev_camera_ctx *ctx = &s_ctx[0];

    if ( ctx->camera_context == NULL ) {
        LOG( LOG_ERR, "Failed to process reg write.camera_init must be called before\n");
        ret = -EINVAL;
        goto Err;
    }

    if (!buf)
        return ret;

    buf_orig = kstrdup(buf, GFP_KERNEL);
    if (!buf_orig)
        return ret;

    parse_param(buf_orig, (char **)&parm);

    if (!parm[0]) {
        ret = -EINVAL;
        goto Err;
    }

    if (!strcmp(parm[0], "r")) {
        if (!parm[1] || (kstrtoul(parm[1], 16, &val) < 0)) {
            ret = -EINVAL;
            goto Err;
        }
        reg_addr = val;
        reg_val = ctx->camera_control.read_sensor_register( ctx->camera_context, reg_addr);
        pr_info("SENSOR READ[0x%04x]=0x%02x\n", reg_addr, reg_val);
    } else if (!strcmp(parm[0], "w")) {
        if (!parm[1] || (kstrtoul(parm[1], 16, &val) < 0)) {
            ret = -EINVAL;
            goto Err;
        }
        reg_addr = val;
        if (!parm[2] || (kstrtoul(parm[2], 16, &val) < 0)) {
            ret = -EINVAL;
            goto Err;
        }
        reg_val = val;
        ctx->camera_control.write_sensor_register( ctx->camera_context, reg_addr, reg_val);
        pr_info("SENSOR WRITE[0x%04x]=0x%02x\n", reg_addr, reg_val);
        reg_val = ctx->camera_control.read_sensor_register( ctx->camera_context, reg_addr);
        pr_info("SENSOR READ[0x%04x]=0x%02x\n", reg_addr, reg_val);
    } else if (!strcmp(parm[0], "d")) {
        if (!parm[1] || (kstrtoul(parm[1], 16, &val) < 0)) {
            ret = -EINVAL;
            goto Err;
        }
        reg_addr = val;
        if (!parm[2] || (kstrtoul(parm[2], 10, &val) < 0))
            val = 1;

        for (i = 0; i < val; i++) {
            reg_val = ctx->camera_control.read_sensor_register( ctx->camera_context, reg_addr);
            pr_info("SENSOR DUMP[0x%04x]=0x%02x\n", reg_addr, reg_val);
            reg_addr += 1;
        }
    } else
        pr_info("unsupprt cmd!\n");
Err:
    kfree(buf_orig);
    return ret;
}
static DEVICE_ATTR(sreg, S_IRUGO | S_IWUSR, sreg_read, sreg_write);


static ssize_t info_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
    switch (info_idx) {
        case SEN_RESOLUTION:
            return sprintf(buf, "%s/%dx%d\n", ConversionTable[0][cur_idx].res, ConversionTable[0][cur_idx].w, ConversionTable[0][cur_idx].h);
        case SEN_NAME:
            return sprintf(buf, "%s\n", ConversionTable[0][cur_idx].sensor_name);
        default:
            return sprintf(buf, "%s/%dx%d\n", ConversionTable[0][cur_idx].res, ConversionTable[0][cur_idx].w, ConversionTable[0][cur_idx].h);
    }
}

static ssize_t info_write(
    struct device *dev, struct device_attribute *attr,
    char const *buf, size_t size)
{
    char *buf_orig = NULL;
    char *parm[8] = {NULL};
    ssize_t ret = size;

    subdev_camera_ctx *ctx = &s_ctx[0];

    if ( ctx->camera_context == NULL ) {
        LOG( LOG_ERR, "Failed to process reg write.camera_init must be called before\n");
        ret = -EINVAL;
        goto Err;
    }

    if (!buf)
        return ret;

    buf_orig = kstrdup(buf, GFP_KERNEL);
    if (!buf_orig)
        return ret;

    parse_param(buf_orig, (char **)&parm);

    if (!parm[0]) {
        ret = -EINVAL;
        goto Err;
    }

    if (!strcmp(parm[0], "resolution")) {
        info_idx = SEN_RESOLUTION;
    } else if (!strcmp(parm[0], "name")) {
        info_idx = SEN_NAME;
    } else
        pr_info("unsupprt cmd!\n");
Err:
    kfree(buf_orig);
    return ret;
}

static DEVICE_ATTR(info, S_IRUGO | S_IWUSR, info_read, info_write);

static int camera_init( struct v4l2_subdev *sd, u32 val )
{
    int rc = 0;

    if ( val < FIRMWARE_CONTEXT_NUMBER && SOC_SENSOR_SENSOR_ENTRY_ARR[val] ) {

        s_ctx[val].camera_context = NULL;

        memset(&( s_ctx[val].camera_control), 0, sizeof(sensor_control_t));
        ( SOC_SENSOR_SENSOR_ENTRY_ARR[val] )( &( s_ctx[val].camera_context ), &( s_ctx[val].camera_control ), (void*)sensor_bp );

        if ( s_ctx[val].camera_context == NULL ) {
            LOG( LOG_ERR, "Failed to process camera_init for ctx:%d. Sensor is not initialized yet. camera_init must be called before", val );
            rc = -1;
            return rc;
        }

        LOG( LOG_INFO, "Sensor has been initialized for ctx:%d\n", val );
    } else {
        rc = -1;
        LOG( LOG_ERR, "Failed to process camera init for ctx:%d", val );
    }
    return rc;
}

static int camera_reset( struct v4l2_subdev *sd, u32 val )
{
    int rc = 0;

    if (val)
        return rc;

    if ( val < FIRMWARE_CONTEXT_NUMBER && SOC_SENSOR_SENSOR_RESET_ARR[val] ) {
        ( SOC_SENSOR_SENSOR_RESET_ARR[val] )( s_ctx[val].camera_context );
    } else {
        rc = -1;
        LOG( LOG_ERR, "Failed to process camera reset for ctx:%d", val );
    }

    LOG( LOG_INFO, "Sensor has been reset for ctx:%d\n", val );
    return rc;
}

int camera_notify( uint notification, void *arg)
{
    v4l2_subdev_notify(&soc_sensor, notification,arg);

    return 0;
}

static long camera_ioctl( struct v4l2_subdev *sd, unsigned int cmd, void *arg )
{
    long rc = 0;

    if ( ARGS_TO_PTR( arg )->ctx_num >= FIRMWARE_CONTEXT_NUMBER ) {
        LOG( LOG_ERR, "Failed to process camera_ioctl for ctx:%d\n", ARGS_TO_PTR( arg )->ctx_num );
        return -1;
    }

    subdev_camera_ctx *ctx = &s_ctx[ARGS_TO_PTR( arg )->ctx_num];

    if ((ctx->camera_context == NULL) || (ctx->camera_control.get_parameters == NULL)) {
        LOG( LOG_ERR, "Failed to process camera_ioctl. Sensor is not initialized yet. camera_init must be called before" );
        rc = -1;
        return rc;
    }


    const sensor_param_t *params = ctx->camera_control.get_parameters( ctx->camera_context );

    switch ( cmd ) {
    case SOC_SENSOR_STREAMING_ON:
        ctx->camera_control.start_streaming( ctx->camera_context );
        break;
    case SOC_SENSOR_STREAMING_OFF:
        ctx->camera_control.stop_streaming( ctx->camera_context );
        break;
    case SOC_SENSOR_SET_PRESET:
        ctx->camera_control.set_mode( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_SET_TEST_PATTERN:
        ctx->camera_control.sensor_test_pattern( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_ALLOC_AGAIN:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->camera_control.alloc_analog_gain( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_ALLOC_DGAIN:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->camera_control.alloc_digital_gain( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_ALLOC_IT:
        ctx->camera_control.alloc_integration_time( ctx->camera_context, &ARGS_TO_PTR( arg )->args.integration_time.it_short, &ARGS_TO_PTR( arg )->args.integration_time.it_medium, &ARGS_TO_PTR( arg )->args.integration_time.it_long );
        break;
    case SOC_SENSOR_UPDATE_EXP:
        ctx->camera_control.sensor_update( ctx->camera_context );
        break;
    case SOC_SENSOR_READ_REG:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->camera_control.read_sensor_register( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_SENSOR_WRITE_REG:
        ctx->camera_control.write_sensor_register( ctx->camera_context, ARGS_TO_PTR( arg )->args.general.val_in, ARGS_TO_PTR( arg )->args.general.val_in2 );
        break;
    case SOC_SENSOR_GET_PRESET_NUM:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->modes_num;
        break;
    case SOC_SENSOR_GET_PRESET_CUR:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->mode;
        break;
    case SOC_SENSOR_GET_PRESET_WIDTH: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].resolution.width;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_PRESET_HEIGHT: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].resolution.height;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_PRESET_FPS: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].fps;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_SENSOR_BITS: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].bits;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_PRESET_EXP: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].exposures;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_PRESET_MODE: {
        int preset = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( preset < params->modes_num ) {
            ARGS_TO_PTR( arg )
                ->args.general.val_out = params->modes_table[preset].wdr_mode;
        } else {
            LOG( LOG_ERR, "Preset number is invalid. Available %d presets, requested %d", params->modes_num, preset );
            rc = -1;
        }
    } break;
    case SOC_SENSOR_GET_EXP_NUMBER:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->sensor_exp_number;
        break;
    case SOC_SENSOR_GET_INTEGRATION_TIME_MAX:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_max;
        break;
    case SOC_SENSOR_GET_INTEGRATION_TIME_MIN:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_min;
        break;
    case SOC_SENSOR_GET_INTEGRATION_TIME_LONG_MAX:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_long_max;
        break;
    case SOC_SENSOR_GET_INTEGRATION_TIME_LIMIT:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_limit;
        break;
    case SOC_SENSOR_GET_ANALOG_GAIN_MAX:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->again_log2_max;
        break;
    case SOC_SENSOR_GET_DIGITAL_GAIN_MAX:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->dgain_log2_max;
        break;
    case SOC_SENSOR_GET_UPDATE_LATENCY:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->integration_time_apply_delay;
        break;
    case SOC_SENSOR_GET_LINES_PER_SECOND:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->lines_per_second;
        break;
    case SOC_SENSOR_GET_FPS: {
        int mode = ARGS_TO_PTR( arg )->args.general.val_in;
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->modes_table[mode].fps;
    } break;
    case SOC_SENSOR_GET_ACTIVE_HEIGHT: {
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->active.height;
    } break;
    case SOC_SENSOR_GET_ACTIVE_WIDTH: {
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->active.width;
    } break;
    case SOC_SENSOR_GET_BAYER_PATTERN: {
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->bayer;
    } break;
    case SOC_SENSOR_GET_SENSOR_NAME: {
        memcpy(ARGS_TO_PTR( arg )
            ->s_name.name, sensor_name[ARGS_TO_PTR( arg )->ctx_num], strlen(sensor_name[ARGS_TO_PTR( arg )->ctx_num]));
        ARGS_TO_PTR( arg )
            ->s_name.name_len = strlen(sensor_name[ARGS_TO_PTR( arg )->ctx_num]);
    } break;
    case SOC_SENSOR_GET_CONTEXT_SEQ: {
        ARGS_TO_PTR( arg )
            ->isp_context_seq.sequence = params->isp_context_seq.sequence;
        if ( isp_seq_num == -1 || ( params->isp_context_seq.seq_table_max- 1 ) < isp_seq_num ) {
           ARGS_TO_PTR( arg )
               ->isp_context_seq.seq_num = params->isp_context_seq.seq_num;
           LOG( LOG_ERR, "Warning: wrong isp seq num, isp_seq_num range = 0 - %d\n, set isp_seq_num = 0\n", ( params->isp_context_seq.seq_table_max- 1 ) );
        } else {
           ARGS_TO_PTR( arg )
               ->isp_context_seq.seq_num = isp_seq_num;
           LOG( LOG_ERR, "get isp_seq_num = %d\n", isp_seq_num );
        }
    } break;
    case SOC_SENSOR_IR_CUT_SET: {
        int32_t preset = ARGS_TO_PTR( arg )->args.general.val_in;
        ctx->camera_control.ir_cut_set(ctx->camera_context,preset);
    } break;
    case SOC_SENSOR_GET_ID: {
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->camera_control.get_id(ctx->camera_context);
    } break;
    case SOC_SENSOR_VMAX_FPS_GET: {
        if ( ctx->camera_control.vmax_fps )
            ARGS_TO_PTR( arg )
                ->args.general.val_out = ctx->camera_control.vmax_fps(ctx->camera_context, rc);
        else
            ARGS_TO_PTR( arg )
                ->args.general.val_out = 0;
    } break;
    case SOC_SENSOR_VMAX_FPS_SET: {
        uint32_t framerate = ARGS_TO_PTR( arg )->args.general.val_in;
        if ( ctx->camera_control.vmax_fps )
            ctx->camera_control.vmax_fps(ctx->camera_context, framerate);
    } break;
    case SOC_SENSOR_POWER_ON: {
        if ( ctx->camera_control.power_on )
            ctx->camera_control.power_on(ctx->camera_context);
    } break;
    case SOC_SENSOR_POWER_OFF: {
        if ( ctx->camera_control.power_off )
            ctx->camera_control.power_off(ctx->camera_context);
    } break;
    case SOC_SENSOR_SET_DCAM_MODE: {
        int32_t preset = ARGS_TO_PTR( arg )->args.general.val_in;
        ctx->camera_control.dcam_mode(ctx->camera_context,preset);
    } break;

    default:
        LOG( LOG_WARNING, "Unknown soc sensor ioctl cmd %d", cmd );
        rc = -1;
        break;
    };

    return rc;
}


static const struct v4l2_subdev_core_ops core_ops = {
    .log_status = camera_log_status,
    .init = camera_init,
    .reset = camera_reset,
    .ioctl = camera_ioctl,
};


static const struct v4l2_subdev_ops camera_ops = {
    .core = &core_ops,
};

static int32_t ir_cut_get_named_gpio(struct device_node *np)
{
    int i;
    int gcount = 0; // ir cut gpio count
    int gname = 0; // ir cut gpio name

    memset(sensor_bp->ir_gname, 0, sizeof(sensor_bp->ir_gname)); //init sensor_bp->ir_gname
    sensor_bp->ir_gcount = 0;

    gcount = of_gpio_named_count(np,"ir_cut_gpio");

    if (gcount > IR_CUT_GPIO_MAX_NUM) {
        gcount = IR_CUT_GPIO_MAX_NUM;
        LOG( LOG_WARNING, "dts config gpio numbers do not match IR_CUT_GPIO_MAX_NUM");
    }

    sensor_bp->ir_gcount = gcount;
    LOG(LOG_ERR, "ir cut gpio count = %d\n", gcount);

    for (i = 0; i < gcount; i++) {
        gname = of_get_named_gpio_flags(np,"ir_cut_gpio",i,NULL);
        sensor_bp->ir_gname[i] = gname;
        LOG(LOG_ERR, "ir cut gpio name [%d] = %d\n", i, sensor_bp->ir_gname[i]);
     }
     return 0;
}

int32_t mipi_power_on(struct device *dev)
{
    int ret = -1;

    ret = pm_runtime_get_sync(dev);
    if (ret < 0)
        LOG(LOG_CRIT,"runtime get power error\n");
    return ret;
}

int32_t mipi_power_down(struct device *dev)
{
    int ret = -1;
    ret = pm_runtime_put_sync(dev);
    if (ret < 0)
        LOG(LOG_CRIT,"runtime put power error\n");

    return ret;
}

int32_t mclk_on(struct device *dev)
{
    int ret;
    struct clk *clk;
    int clk_val;
    clk = devm_clk_get(sensor_bp->dev, "mclk_0");
    if (IS_ERR(clk)) {
        pr_err("cannot get mclk_0 clk\n");
        clk = NULL;
        return -1;
    }
    ret = clk_prepare_enable(clk);
    if (ret < 0)
        pr_err(" clk_prepare_enable failed\n");
    clk_val = clk_get_rate(clk);
    pr_err("resume mclock %d MHZ\n",clk_val/1000000);

    return ret;
}

int32_t mclk_down(struct device *dev)
{
    struct clk *clk;

    clk = devm_clk_get(dev, "mclk_0");
    if (IS_ERR(clk)) {
        pr_err("cannot get mclk_0 clk\n");
        clk = NULL;
        return -1;
    }
    clk_disable_unprepare(clk);
    devm_clk_put(dev, clk);

    return 0;
}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void sensor_early_suspend(struct early_suspend *h)
{
    mclk_down((struct device *)h->param);
}

static void sensor_late_resume(struct early_suspend *h)
{
    mclk_on((struct device *)h->param);
}
#endif

static int32_t soc_sensor_probe( struct platform_device *pdev )
{
    int32_t rc = 0;
    int rtn = 0;
    int i,j;
    int prst_res = 0;
    struct device *dev = &pdev->dev;
    struct device_node *dev_np = dev->of_node;

#if PLATFORM_C308X
    write_reg(0xdfff,0xfe000608);        // set GPIOA_13 output
    write_reg(0xdfff,0xfe000604);        // pull down GPIOA_13
#endif
    pm_runtime_enable(&pdev->dev);
    mipi_power_on(&pdev->dev);

    sensor_bp = kzalloc( sizeof( *sensor_bp ), GFP_KERNEL );
    if (sensor_bp == NULL) {
        LOG(LOG_ERR, "Failed to alloc mem\n");
        return -ENOMEM;
    }

    rtn = of_property_read_string(dev->of_node, "sensor-name", &sensor_name[0]);
    if (rtn != 0) {
        pr_err("%s: failed to get sensor name\n", __func__);
    }

    if (sensor)
        sensor_name[0] = sensor;
    pr_err("config sensor %s driver.\n", sensor_name[0]);

#if FIRMWARE_CONTEXT_NUMBER >= 2
    rtn = of_property_read_string(dev->of_node, "sensor-name-sub", &sensor_name[1]);
    if (rtn != 0) {
        pr_err("%s: failed to get sensor name sub\n", __func__);
        sensor_name[1] = "imx290sub";
    }
    if (sensor_name[1])
        pr_err("config sensor sub %s driver.\n", sensor_name[1]);
#endif

#if FIRMWARE_CONTEXT_NUMBER >= 3
    rtn = of_property_read_string(dev->of_node, "sensor-name-ssub", &sensor_name[2]);
    if (rtn != 0) {
        pr_err("%s: failed to get sensor name ssub\n", __func__);
        sensor_name[2] = "imx290ssub";
    }
    if (sensor_name[2])
        pr_err("config sensor ssub %s driver.\n", sensor_name[2]);
#endif

#if FIRMWARE_CONTEXT_NUMBER == 4
    rtn = of_property_read_string(dev->of_node, "sensor-name-max", &sensor_name[3]);
    if (rtn != 0) {
        pr_err("%s: failed to get sensor name sssub\n", __func__);
        sensor_name[3] = "imx290sssub";
    }
    if (sensor_name[3])
        pr_err("config sensor ssub %s driver.\n", sensor_name[3]);
#endif

    ir_cut_get_named_gpio(dev_np);
    sensor_bp_init(sensor_bp, dev);

    for (j = 0; j < FIRMWARE_CONTEXT_NUMBER; j++) {
        for (i = 0; i < NELEM(ConversionTable[j]); ++i) {
            if (ConversionTable[j][i].sensor_name && (strcmp(ConversionTable[j][i].sensor_name, sensor_name[j]) == 0)) {
                SOC_SENSOR_SENSOR_ENTRY_ARR[j] = ConversionTable[j][i].sensor_init;
                SOC_SENSOR_SENSOR_RESET_ARR[j] = ConversionTable[j][i].sensor_deinit;
                SOC_SENSOR_SENSOR_DETECT_ARR[j] = ConversionTable[j][i].sensor_detect;
                if (j == 0) {
                    prst_res = ConversionTable[j][i].w * ConversionTable[j][i].h;
                    sensor_idx = i;
                }
                break;
            }
        }

        if (j == 0) {
            if (i == NELEM(ConversionTable[j])) {
                pr_err("Fatal error:cant find %s driver with dts config!\n", sensor_name[j]);
                return -1;
            }
        }
    }

    //only auto detect camera0
    if (SOC_SENSOR_SENSOR_DETECT_ARR[0](sensor_bp) != 0) {
        for (i = 0; i < NELEM(ConversionTable[0]); ++i) {
            if (prst_res < ConversionTable[0][i].w * ConversionTable[0][i].h)
                continue;
            if (ConversionTable[0][i].sensor_name && ConversionTable[0][i].sensor_detect(sensor_bp) == 0) {
                SOC_SENSOR_SENSOR_ENTRY_ARR[0] = ConversionTable[0][i].sensor_init;
                SOC_SENSOR_SENSOR_RESET_ARR[0] = ConversionTable[0][i].sensor_deinit;
                sensor_name[0] = ConversionTable[0][i].sensor_name;
                cur_idx = i;
                break;
            }
        }
        if ( i == NELEM(ConversionTable[0])) {
            LOG(LOG_CRIT, "find no matching sensor.");
            return -1;
        }
    } else {
        cur_idx = sensor_idx;
    }

    v4l2_subdev_init( &soc_sensor, &camera_ops );

    soc_sensor.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

    snprintf( soc_sensor.name, V4L2_SUBDEV_NAME_SIZE, "%s", V4L2_SOC_SENSOR_NAME );

    soc_sensor.dev = &pdev->dev;
    rc = v4l2_async_register_subdev( &soc_sensor );

    device_create_file(dev, &dev_attr_info);
    device_create_file(dev, &dev_attr_sreg);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
    early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
    early_suspend.suspend = sensor_early_suspend;
    early_suspend.resume = sensor_late_resume;
    early_suspend.param = dev;
    register_early_suspend(&early_suspend);
#endif

    LOG( LOG_CRIT, "register v4l2 sensor device. result %d, sd 0x%x sd->dev 0x%x", rc, &soc_sensor, soc_sensor.dev );

    return rc;
}

static int soc_sensor_remove( struct platform_device *pdev )
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
    unregister_early_suspend(&early_suspend);
#endif

    device_remove_file(&pdev->dev, &dev_attr_sreg);
    device_remove_file(&pdev->dev, &dev_attr_info);

    v4l2_async_unregister_subdev( &soc_sensor );

    if (sensor_bp != NULL) {
        kfree(sensor_bp);
        sensor_bp = NULL;
    }
    mipi_power_on(&pdev->dev);
    return 0;
}

static const struct of_device_id sensor_dt_match[] = {
    {.compatible = "soc, sensor"},
    {}};

static struct platform_driver soc_sensor_driver = {
    .probe = soc_sensor_probe,
    .remove = soc_sensor_remove,
    .driver = {
        .name = "soc_sensor",
        .owner = THIS_MODULE,
        .of_match_table = sensor_dt_match,
    },
};

int __init acamera_camera_sensor_init( void )
{
    LOG( LOG_ERR, "Sensor subdevice init" );

    return platform_driver_register( &soc_sensor_driver );
}


void __exit acamera_camera_sensor_exit( void )
{
    LOG( LOG_INFO, "Sensor subdevice exit" );

    platform_driver_unregister( &soc_sensor_driver );
}


module_init( acamera_camera_sensor_init );
module_exit( acamera_camera_sensor_exit );
MODULE_LICENSE( "GPL v2" );
MODULE_AUTHOR( "ARM IVG AC" );
