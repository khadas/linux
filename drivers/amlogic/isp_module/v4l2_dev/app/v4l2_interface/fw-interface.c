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

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <asm/div64.h>
#include <linux/sched.h>
#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>

#include "system_interrupts.h"
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "application_command_api.h"
#include "acamera_logger.h"
#include "isp-v4l2-common.h"
#include "isp-v4l2.h"
#include "fw-interface.h"
#include "runtime_initialization_settings.h"
#include <linux/delay.h>
#include "acamera_fw.h"
#include "acamera_isp_config.h"
#include "fsm_intf.h"
#include "acamera_fsm_mgr.h"
#include "ae_manual_fsm.h"
#include "dma_writer.h"
#include "dma_writer_fsm.h"


//use main_firmware.c routines to initialize fw
extern int isp_fw_init( uint32_t hw_isp_addr );
extern void isp_fw_exit( void );
extern void *acamera_get_api_ctx_ptr( void );
extern void *acamera_get_ctx_ptr( uint32_t ctx_id );

static int isp_started = 0;
static int custom_wdr_mode[FIRMWARE_CONTEXT_NUMBER];
static int custom_exp[FIRMWARE_CONTEXT_NUMBER];
static int custom_fps[FIRMWARE_CONTEXT_NUMBER];
get_cmd_para_t get_cmd_para;
isp_ctl_swhw_registers_cmd_t swhw_registers_cmd;
uint64_t isp_modules_by_pass_get_cmd = 0;

static int isp_fw_do_set_cmd( uint32_t ctx_id, uint8_t command_type, uint8_t command, uint32_t value );
static int isp_fw_do_get_cmd( uint32_t ctx_id, uint8_t command_type, uint8_t command, uint32_t *ret_val );
/* ----------------------------------------------------------------
 * fw_interface control interface
 */
typedef void (*acamera_isp_top_bypass_write_t)(uintptr_t, uint8_t);

acamera_isp_top_bypass_write_t acamera_isp_top_bypass_write[] = {
    NULL,/*set get cmd*/
    acamera_isp_top_bypass_video_test_gen_write,
    acamera_isp_top_bypass_input_formatter_write,
    acamera_isp_top_bypass_decompander_write,
    acamera_isp_top_bypass_sensor_offset_wdr_write,
    acamera_isp_top_bypass_gain_wdr_write,
    acamera_isp_top_bypass_frame_stitch_write,
    acamera_isp_top_bypass_digital_gain_write,
    acamera_isp_top_bypass_frontend_sensor_offset_write,
    acamera_isp_top_bypass_fe_sqrt_write,
    acamera_isp_top_bypass_raw_frontend_write,
    acamera_isp_top_bypass_defect_pixel_write,
    acamera_isp_top_bypass_sinter_write,
    acamera_isp_top_bypass_temper_write,
    acamera_isp_top_bypass_ca_correction_write,
    acamera_isp_top_bypass_square_be_write,
    acamera_isp_top_bypass_sensor_offset_pre_shading_write,
    acamera_isp_top_bypass_radial_shading_write,
    acamera_isp_top_bypass_mesh_shading_write,
    acamera_isp_top_bypass_white_balance_write,
    acamera_isp_top_bypass_iridix_gain_write,
    acamera_isp_top_bypass_iridix_write,
    acamera_isp_top_bypass_mirror_write,
    acamera_isp_top_bypass_demosaic_rgb_write,
    acamera_isp_top_bypass_demosaic_rgbir_write,
    acamera_isp_top_bypass_pf_correction_write,
    acamera_isp_top_bypass_ccm_write,
    acamera_isp_top_bypass_cnr_write,
    acamera_isp_top_bypass_3d_lut_write,
    acamera_isp_top_bypass_nonequ_gamma_write,
    acamera_isp_top_bypass_fr_crop_write,
    acamera_isp_top_bypass_fr_gamma_rgb_write,
    acamera_isp_top_bypass_fr_sharpen_write,
    acamera_isp_top_bypass_fr_cs_conv_write,
    acamera_isp_top_bypass_ds1_crop_write,
    acamera_isp_top_bypass_ds1_scaler_write,
    acamera_isp_top_bypass_ds1_gamma_rgb_write,
    acamera_isp_top_bypass_ds1_sharpen_write,
    acamera_isp_top_bypass_ds1_cs_conv_write,
    acamera_isp_top_isp_raw_bypass_write,
    acamera_isp_top_isp_processing_fr_bypass_mode_write,
};
typedef uint8_t (*acamera_isp_top_bypass_read_t)(uintptr_t);

acamera_isp_top_bypass_read_t acamera_isp_top_bypass_read[] = {
    NULL,
    acamera_isp_top_bypass_video_test_gen_read,
    acamera_isp_top_bypass_input_formatter_read,
    acamera_isp_top_bypass_decompander_read,
    acamera_isp_top_bypass_sensor_offset_wdr_read,
    acamera_isp_top_bypass_gain_wdr_read,
    acamera_isp_top_bypass_frame_stitch_read,
    acamera_isp_top_bypass_digital_gain_read,
    acamera_isp_top_bypass_frontend_sensor_offset_read,
    acamera_isp_top_bypass_fe_sqrt_read,
    acamera_isp_top_bypass_raw_frontend_read,
    acamera_isp_top_bypass_defect_pixel_read,
    acamera_isp_top_bypass_sinter_read,
    acamera_isp_top_bypass_temper_read,
    acamera_isp_top_bypass_ca_correction_read,
    acamera_isp_top_bypass_square_be_read,
    acamera_isp_top_bypass_sensor_offset_pre_shading_read,
    acamera_isp_top_bypass_radial_shading_read,
    acamera_isp_top_bypass_mesh_shading_read,
    acamera_isp_top_bypass_white_balance_read,
    acamera_isp_top_bypass_iridix_gain_read,
    acamera_isp_top_bypass_iridix_read,
    acamera_isp_top_bypass_mirror_read,
    acamera_isp_top_bypass_demosaic_rgb_read,
    acamera_isp_top_bypass_demosaic_rgbir_read,
    acamera_isp_top_bypass_pf_correction_read,
    acamera_isp_top_bypass_ccm_read,
    acamera_isp_top_bypass_cnr_read,
    acamera_isp_top_bypass_3d_lut_read,
    acamera_isp_top_bypass_nonequ_gamma_read,
    acamera_isp_top_bypass_fr_crop_read,
    acamera_isp_top_bypass_fr_gamma_rgb_read,
    acamera_isp_top_bypass_fr_sharpen_read,
    acamera_isp_top_bypass_fr_cs_conv_read,
    acamera_isp_top_bypass_ds1_crop_read,
    acamera_isp_top_bypass_ds1_scaler_read,
    acamera_isp_top_bypass_ds1_gamma_rgb_read,
    acamera_isp_top_bypass_ds1_sharpen_read,
    acamera_isp_top_bypass_ds1_cs_conv_read,
    acamera_isp_top_isp_raw_bypass_read,
    acamera_isp_top_isp_processing_fr_bypass_mode_read,
};

void custom_initialization( uint32_t ctx_num )
{

}


/* fw-interface isp control interface */
int fw_intf_isp_init(  uint32_t hw_isp_addr )
{

    LOG( LOG_INFO, "Initializing fw ..." );
    int rc = 0;
    /* flag variable update */

    rc = isp_fw_init(hw_isp_addr);

    if ( rc == 0 )
        isp_started = 1;

    memset(custom_wdr_mode, 0, sizeof(int) * FIRMWARE_CONTEXT_NUMBER);
    memset(custom_exp, 0, sizeof(int) * FIRMWARE_CONTEXT_NUMBER);
    memset(custom_fps, 0, sizeof(int) * FIRMWARE_CONTEXT_NUMBER);

    return rc;
}

int fw_intf_is_isp_started( void )
{
    return isp_started;
}

void fw_intf_isp_deinit( void )
{
    LOG( LOG_INFO, "Deinitializing fw interface ..." );

    /* flag variable update */
    isp_started = 0;

    isp_fw_exit();
}

uint32_t fw_calibration_update( uint32_t ctx_id )
{
    uint32_t retval;
    acamera_command( ctx_id, TSYSTEM, CALIBRATION_UPDATE, UPDATE, COMMAND_SET, &retval );
    return retval;
}

int fw_intf_isp_start( void )
{
    LOG( LOG_DEBUG, "Starting context" );


    return 0;
}

void fw_intf_isp_stop( void )
{
    LOG( LOG_DEBUG, "Enter" );
}

int fw_intf_isp_set_current_ctx_id( uint32_t ctx_id )
{
    int active_ctx_id = ctx_id;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

#if defined( TGENERAL ) && defined( ACTIVE_CONTEXT )
    acamera_command( ctx_id, TGENERAL, ACTIVE_CONTEXT, ctx_id, COMMAND_SET, &active_ctx_id );
#endif

    return active_ctx_id;
}

int fw_intf_isp_get_current_ctx_id( uint32_t ctx_id )
{
    int active_ctx_id = -1;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

#if defined( TGENERAL ) && defined( ACTIVE_CONTEXT )
    acamera_command( ctx_id, TGENERAL, ACTIVE_CONTEXT, 0, COMMAND_GET, &active_ctx_id );
#endif

    return active_ctx_id;
}

int fw_intf_isp_get_sensor_info( uint32_t ctx_id, isp_v4l2_sensor_info *sensor_info )
{
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

#if defined( TSENSOR ) && defined( SENSOR_SUPPORTED_PRESETS ) && defined( SENSOR_INFO_PRESET ) && \
    defined( SENSOR_INFO_FPS ) && defined( SENSOR_INFO_WIDTH ) && defined( SENSOR_INFO_HEIGHT )
    int i, j;
    uint32_t ret_val;
    uint32_t preset_num;

    /* reset buffer */
    memset( sensor_info, 0x0, sizeof( isp_v4l2_sensor_info ) );

    /* get preset size */
    acamera_command( ctx_id, TSENSOR, SENSOR_SUPPORTED_PRESETS, 0, COMMAND_GET, &preset_num );
    if ( preset_num > MAX_SENSOR_PRESET_SIZE ) {
        LOG( LOG_ERR, "MAX_SENSOR_PRESET_SIZE is too small ! (preset_num = %d)", preset_num );
        preset_num = MAX_SENSOR_PRESET_SIZE;
    }

    /* fill preset values */
    for ( i = 0; i < preset_num; i++ ) {
        uint32_t width, height, fps, wdrmode, exposures = 1;

        /* get next preset */
        acamera_command( ctx_id, TSENSOR, SENSOR_INFO_PRESET, i, COMMAND_SET, &ret_val );
        acamera_command( ctx_id, TSENSOR, SENSOR_INFO_FPS, i, COMMAND_GET, &fps );
        acamera_command( ctx_id, TSENSOR, SENSOR_INFO_WIDTH, i, COMMAND_GET, &width );
        acamera_command( ctx_id, TSENSOR, SENSOR_INFO_HEIGHT, i, COMMAND_GET, &height );
#if defined( SENSOR_INFO_EXPOSURES )
        acamera_command( ctx_id, TSENSOR, SENSOR_INFO_EXPOSURES, i, COMMAND_GET, &exposures );
#endif
        acamera_command( ctx_id, TSENSOR, SENSOR_INFO_WDR_MODE, i, COMMAND_GET, &wdrmode );
        /* find proper index from sensor_info */
        for ( j = 0; j < sensor_info->preset_num; j++ ) {
            if ( sensor_info->preset[j].width == width &&
                 sensor_info->preset[j].height == height )
                break;
        }

        /* store preset */
        if ( sensor_info->preset[j].fps_num < MAX_SENSOR_FPS_SIZE ) {
            sensor_info->preset[j].width = width;
            sensor_info->preset[j].height = height;
            sensor_info->preset[j].exposures[sensor_info->preset[j].fps_num] = exposures;
            sensor_info->preset[j].fps[sensor_info->preset[j].fps_num] = fps;
            sensor_info->preset[j].idx[sensor_info->preset[j].fps_num] = i;
            sensor_info->preset[j].wdr_mode[sensor_info->preset[j].fps_num] = wdrmode;
            sensor_info->preset[j].fps_num++;
            if ( sensor_info->preset_num <= j )
                sensor_info->preset_num++;
        } else {
            LOG( LOG_ERR, "FPS number overflowed ! (preset#%d / index#%d)", i, j );
        }
    }
    uint32_t spreset, swidth, sheight;
    acamera_command( ctx_id, TSENSOR, SENSOR_PRESET, 0, COMMAND_GET, &spreset );
    acamera_command( ctx_id, TSENSOR, SENSOR_WIDTH, 0, COMMAND_GET, &swidth );
    acamera_command( ctx_id, TSENSOR, SENSOR_HEIGHT, 0, COMMAND_GET, &sheight );

    for ( i = 0; i < sensor_info->preset_num; i++ ) {
        if ( swidth == sensor_info->preset[i].width && sheight == sensor_info->preset[i].height ) {
            sensor_info->preset_cur = i;
            break;
        }
    }

    if ( i < MAX_SENSOR_PRESET_SIZE ) {
        for ( j = 0; j < sensor_info->preset[i].fps_num; j++ ) {
            if ( sensor_info->preset[i].idx[j] == spreset ) {
                sensor_info->preset[i].fps_cur = spreset;
                break;
            }
        }
    }

    //check current sensor settings
    for ( i = 0; i < sensor_info->preset_num; i++ ) {
        LOG( LOG_DEBUG, "   Idx#%02d - W:%04d H:%04d",
             i, sensor_info->preset[i].width, sensor_info->preset[i].height );
        for ( j = 0; j < sensor_info->preset[i].fps_num; j++ )
            LOG( LOG_DEBUG, "            FPS#%d: %d (preset index = %d) exposures:%d  wdr_mode:%d",
                 j, sensor_info->preset[i].fps[j] / 256, sensor_info->preset[i].idx[j],
                 sensor_info->preset[i].exposures[j], sensor_info->preset[i].wdr_mode[j] );
    }
#else
    /* returning default setting (1080p) */
    LOG( LOG_ERR, "API not found, initializing sensor_info to default." );
    sensor_info->preset_num = 1;
    sensor_info->preset[0].fps = 30 * 256;
    sensor_info->preset[0].width = 1920;
    sensor_info->preset[0].height = 1080;
#endif

    return 0;
}

int fw_intf_isp_get_sensor_preset( uint32_t ctx_id )
{
    int value = -1;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

#if defined( TSENSOR ) && defined( SENSOR_PRESET )
    acamera_command( ctx_id, TSENSOR, SENSOR_PRESET, 0, COMMAND_GET, &value );
#endif

    return value;
}

int fw_intf_isp_get_queue_status( void )
{
    return acamera_api_get_queue_status();
}

int fw_intf_isp_set_sensor_preset( uint32_t ctx_id, uint32_t preset )
{
    int value = -1;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

#if defined( TSENSOR ) && defined( SENSOR_PRESET )
    acamera_command( ctx_id, TSENSOR, SENSOR_PRESET, preset, COMMAND_SET, &value );
#endif

    return value;
}

/*
 * fw-interface per-stream control interface
 */
int fw_intf_stream_start( uint32_t ctx_id, isp_v4l2_stream_type_t streamType )
{
    uint32_t rc = 0;

#if ISP_DMA_RAW_CAPTURE && ISP_HAS_RAW_CB
    if ( streamType == V4L2_STREAM_TYPE_RAW ) {
#if defined( TFPGA ) && defined( DMA_RAW_CAPTURE_ENABLED_ID )
        uint32_t ret_val;
        acamera_command( ctx_id, TFPGA, DMA_RAW_CAPTURE_ENABLED_ID, ON, COMMAND_SET, &ret_val );
#else
        LOG( LOG_CRIT, "no api for DMA_RAW_CAPTURE_ENABLED_ID" );
#endif
    }
#endif
    LOG( LOG_ERR, "Starting stream type %d", streamType );
    acamera_command( ctx_id, TSENSOR, SENSOR_STREAMING, ON, COMMAND_SET, &rc );

    uint32_t dma_type = dma_max;

#if ISP_HAS_SC0
    if (streamType == V4L2_STREAM_TYPE_SC0)
        dma_type = dma_sc0;
#endif
#if ISP_HAS_SC1
    if (streamType == V4L2_STREAM_TYPE_SC1)
        dma_type = dma_sc1;
#endif
#if ISP_HAS_SC2
    if (streamType == V4L2_STREAM_TYPE_SC2)
        dma_type = dma_sc2;
#endif
#if ISP_HAS_SC3
    if (streamType == V4L2_STREAM_TYPE_CROP)
        dma_type = dma_sc3;
#endif

    uint32_t ret_val;
    LOG( LOG_ERR, "Starting stream type %d", dma_type );
    acamera_command( ctx_id, TAML_SCALER, SCALER_STREAMING_ON, ON | (dma_type << 16), COMMAND_SET, &ret_val );

    return 0;
}

void fw_intf_stream_stop( uint32_t ctx_id, isp_v4l2_stream_type_t streamType, int stream_on_count )
{
    uint32_t rc = 0;
    LOG( LOG_DEBUG, "Stopping stream type %d", streamType );
#if ISP_DMA_RAW_CAPTURE && ISP_HAS_RAW_CB
    if ( streamType == V4L2_STREAM_TYPE_RAW ) {
#if defined( TFPGA ) && defined( DMA_RAW_CAPTURE_ENABLED_ID )
        uint32_t ret_val;
        acamera_command( ctx_id, TFPGA, DMA_RAW_CAPTURE_ENABLED_ID, OFF, COMMAND_SET, &ret_val );
#else
        LOG( LOG_CRIT, "no api for DMA_RAW_CAPTURE_ENABLED_ID" );
#endif
    }
#endif

    if (streamType == V4L2_STREAM_TYPE_FR) {
        if (stream_on_count == 0)
            acamera_command( ctx_id, TSENSOR, SENSOR_STREAMING, OFF, COMMAND_SET, &rc );
        acamera_api_dma_buff_queue_reset(ctx_id, dma_fr);
    } else if (streamType == V4L2_STREAM_TYPE_DS1) {
        if (stream_on_count == 0)
            acamera_command( ctx_id, TSENSOR, SENSOR_STREAMING, OFF, COMMAND_SET, &rc );
        acamera_api_dma_buff_queue_reset(ctx_id, dma_ds1);
    }

    uint32_t dma_type = dma_max;

#if ISP_HAS_SC0
    if (streamType == V4L2_STREAM_TYPE_SC0)
        dma_type = dma_sc0;
#endif
#if ISP_HAS_SC1
    if (streamType == V4L2_STREAM_TYPE_SC1)
        dma_type = dma_sc1;
#endif
#if ISP_HAS_SC2
    if (streamType == V4L2_STREAM_TYPE_SC2)
        dma_type = dma_sc2;
#endif
#if ISP_HAS_SC3
    if (streamType == V4L2_STREAM_TYPE_CROP)
        dma_type = dma_sc3;
#endif

    uint32_t ret_val;
    LOG( LOG_ERR, "Stopping stream type %d", streamType );
    if ( stream_on_count == 0 )
        acamera_command( ctx_id, TSENSOR, SENSOR_STREAMING, OFF, COMMAND_SET, &rc );

    acamera_command( ctx_id, TAML_SCALER, SCALER_STREAMING_OFF, OFF | (dma_type << 16), COMMAND_SET, &ret_val );

    if ( stream_on_count == 0 )
        ((acamera_context_ptr_t)acamera_get_ctx_ptr(ctx_id))->isp_frame_counter = 0;

    LOG( LOG_CRIT, "Stream off %d, user: %d\n",  streamType, stream_on_count);
}

void fw_intf_stream_pause( uint32_t ctx_id, isp_v4l2_stream_type_t streamType, uint8_t bPause )
{
    LOG( LOG_DEBUG, "Pausing/Resuming stream type %d (Flag=%d", streamType, bPause );

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return;
    }

#if ISP_DMA_RAW_CAPTURE && ISP_HAS_RAW_CB
    if ( streamType == V4L2_STREAM_TYPE_RAW ) {
#if defined( TFPGA ) && defined( DMA_RAW_CAPTURE_ENABLED_ID )
        uint32_t ret_val;
        acamera_command( ctx_id, TFPGA, DMA_RAW_CAPTURE_WRITEON_ID, ( bPause ? 0 : 1 ), COMMAND_SET, &ret_val );
#else
        LOG( LOG_CRIT, "no api for DMA_RAW_CAPTURE_WRITEON_ID" );
#endif
    }
#endif

#if defined( TGENERAL ) && defined( DMA_WRITER_SINGLE_FRAME_MODE )
#if ISP_HAS_RAW_CB
    if ( streamType == V4L2_STREAM_TYPE_RAW ) {
        uint32_t ret_val;

        acamera_command( ctx_id, TGENERAL, DMA_WRITER_SINGLE_FRAME_MODE, bPause ? ON : OFF, COMMAND_SET, &ret_val );
    }
#endif
#endif
}

/* fw-interface sensor hw stream control interface */
int fw_intf_sensor_pause( uint32_t ctx_id )
{
    uint32_t rc = 0;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    application_command( TSENSOR, SENSOR_STREAMING, OFF, COMMAND_SET, &rc );

    return rc;
}

int fw_intf_sensor_resume( uint32_t ctx_id )
{
    uint32_t rc = 0;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    application_command( TSENSOR, SENSOR_STREAMING, ON, COMMAND_SET, &rc );

    return rc;
}

uint32_t fw_intf_find_proper_present_idx(uint32_t ctx_id, const isp_v4l2_sensor_info *sensor_info, int w, int h, uint32_t* fps)
{
  /* search resolution from preset table
     *   for now, use the highest fps.
     *   this should be changed properly in the future to pick fps from application
     */
    int i, j;
    uint32_t idx = 0;

    for ( i = 0; i < sensor_info->preset_num; i++ ) {
        if ( sensor_info->preset[i].width >= w && sensor_info->preset[i].height >= h ) {
            idx = sensor_info->preset[i].idx[0];
            *fps = sensor_info->preset[i].fps[0];
            if (custom_wdr_mode[ctx_id] == 0) {
               *( (char *)&sensor_info->preset[i].fps_cur ) = 0;
               for ( j = 0; j < sensor_info->preset[i].fps_num; j++ ) {
                   if ( sensor_info->preset[i].wdr_mode[j] == custom_wdr_mode[ctx_id] ) {
                       if (0 != custom_fps[ctx_id]) {
                           if ( sensor_info->preset[i].fps[j] == custom_fps[ctx_id] * 256) {
                               *fps = sensor_info->preset[i].fps[j];
                               idx = sensor_info->preset[i].idx[j];
                               *( (char *)&sensor_info->preset[i].fps_cur ) = j;
                               break;
                           }
                       } else {
                           if ( sensor_info->preset[i].fps[j] > (*fps)) {
                               *fps = sensor_info->preset[i].fps[j];
                               idx = sensor_info->preset[i].idx[j];
                               *( (char *)&sensor_info->preset[i].fps_cur ) = j;
                           }
                       }
                   }
               }
               break;
            } else if ((custom_wdr_mode [ctx_id]== 1) || (custom_wdr_mode [ctx_id]== 2)) {
               for (j = 0; j < sensor_info->preset[i].fps_num; j++) {
                  if ((sensor_info->preset[i].exposures[j] == custom_exp[ctx_id]) &&
                     (sensor_info->preset[i].wdr_mode[j] == custom_wdr_mode[ctx_id])) {
                     if (0 != custom_fps[ctx_id]) {
                         if ( sensor_info->preset[i].fps[j] == custom_fps[ctx_id] * 256) {
                             *fps = sensor_info->preset[i].fps[j];
                             idx = sensor_info->preset[i].idx[j];
                             *( (char *)&sensor_info->preset[i].fps_cur ) = j;
                             break;
                         }
                     } else {
                         if ( sensor_info->preset[i].fps[j] >= (*fps)) {
                             idx = sensor_info->preset[i].idx[j];
                             *fps = sensor_info->preset[i].fps[j];
                             *( (char *)&sensor_info->preset[i].fps_cur ) = j;
                         }
                     }
                  }
               }
               break;
            } else {
               LOG( LOG_ERR, "Not Support wdr mode\n");
            }
        }
    }

    if ( i >= sensor_info->preset_num ) {
        LOG( LOG_CRIT, "invalid resolution (width = %d, height = %d)\n", w, h);
        return -1;
    }

    custom_wdr_mode[ctx_id] = sensor_info->preset[i].wdr_mode[sensor_info->preset[i].fps_cur];
    custom_exp[ctx_id] = sensor_info->preset[i].exposures[sensor_info->preset[i].fps_cur];
    custom_fps[ctx_id] = sensor_info->preset[i].fps[sensor_info->preset[i].fps_cur] / 256;

    return idx;
}

/* fw-interface per-stream config interface */
int fw_intf_stream_set_resolution( uint32_t ctx_id, const isp_v4l2_sensor_info *sensor_info,
                                   isp_v4l2_stream_type_t streamType, uint32_t *width, uint32_t *height )
{
    /*
     * StreamType
     *   - FR : directly update sensor resolution since FR doesn't have down-scaler.
     *   - DS : need to be implemented.
     *   - RAW: can't be configured separately from FR.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    LOG( LOG_DEBUG, "streamtype:%d, w:%d h:%d\n", streamType, *width, *height );
    if ( streamType == V4L2_STREAM_TYPE_FR ) {
#if defined( TSENSOR ) && defined( SENSOR_PRESET )
        int result;
        uint32_t ret_val;
        uint32_t idx = 0x0;
        uint32_t fps = 0x0;
        uint32_t w, h;

        w = *width;
        h = *height;

        uint32_t width_cur, height_cur, exposure_cur, wdr_mode_cur, fps_cur;
        wdr_mode_cur = 0;
        exposure_cur = 0;
        fps_cur = 0;
        //check if we need to change sensor preset
        acamera_command( ctx_id, TSENSOR, SENSOR_WIDTH, 0, COMMAND_GET, &width_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_HEIGHT, 0, COMMAND_GET, &height_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_EXPOSURES, 0, COMMAND_GET, &exposure_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_WDR_MODE, 0, COMMAND_GET, &wdr_mode_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_FPS, 0, COMMAND_GET, &fps_cur );
        LOG( LOG_DEBUG, "target (width = %d, height = %d, fps = %d) current (w=%d h=%d exposure_cur = %d wdr_mode_cur = %d, fps = %d)",
        w, h, custom_fps, width_cur, height_cur, exposure_cur, wdr_mode_cur, fps_cur / 256);

        if ( width_cur != w || height_cur != h || exposure_cur != custom_exp[ctx_id] || wdr_mode_cur != custom_wdr_mode[ctx_id] || fps_cur / 256 != custom_fps[ctx_id]) {

            idx = fw_intf_find_proper_present_idx(ctx_id, sensor_info, w, h, &fps);

            /* set sensor resolution preset */
            LOG( LOG_CRIT, "Setting new resolution : width = %d, height = %d (preset idx = %d, fps = %d)", w, h, idx, fps / 256 );
            result = acamera_command( ctx_id, TSENSOR, SENSOR_PRESET, idx, COMMAND_SET, &ret_val );
            *( (char *)&sensor_info->preset_cur ) = idx;
            if ( result ) {
                LOG( LOG_CRIT, "Failed to set preset to %u, ret_value: %d.", idx, result );
                return -EINVAL;
            }
        } else {
            acamera_command( ctx_id, TSENSOR, SENSOR_PRESET, 0, COMMAND_GET, &idx );
            LOG( LOG_CRIT, "Leaving same sensor settings resolution : width = %d, height = %d (preset idx = %d)", w, h, idx );
        }
#endif
    }
#if ISP_HAS_DS1
    else if ( streamType == V4L2_STREAM_TYPE_DS1 ) {

        int result;
        uint32_t ret_val;
        uint32_t w, h;

        w = *width;
        h = *height;

        uint32_t width_cur, height_cur;
        //check if we need to change sensor preset
        acamera_command( ctx_id, TSENSOR, SENSOR_WIDTH, 0, COMMAND_GET, &width_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_HEIGHT, 0, COMMAND_GET, &height_cur );
        if ( w > width_cur || h > height_cur ) {
            LOG( LOG_ERR, "Invalid target size: (width = %d, height = %d), current (w=%d h=%d)", w, h, width_cur, height_cur );
            return -EINVAL;
        }

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_TYPE_ID ) && defined( IMAGE_RESIZE_WIDTH_ID )
        {
            result = acamera_command( ctx_id, TIMAGE, IMAGE_RESIZE_TYPE_ID, SCALER, COMMAND_SET, &ret_val );
            if ( result ) {
                LOG( LOG_CRIT, "Failed to set resize_type, ret_value: %d.", result );
                return result;
            }

            result = acamera_command( ctx_id, TIMAGE, IMAGE_RESIZE_WIDTH_ID, w, COMMAND_SET, &ret_val );
            if ( result ) {
                LOG( LOG_CRIT, "Failed to set resize_width, ret_value: %d.", result );
                return result;
            }
            result = acamera_command( ctx_id, TIMAGE, IMAGE_RESIZE_HEIGHT_ID, h, COMMAND_SET, &ret_val );
            if ( result ) {
                LOG( LOG_CRIT, "Failed to set resize_height, ret_value: %d.", result );
                return result;
            }

            result = acamera_command( ctx_id, TIMAGE, IMAGE_RESIZE_ENABLE_ID, RUN, COMMAND_SET, &ret_val );
            if ( result ) {
                LOG( LOG_CRIT, "Failed to set resize_enable, ret_value: %d.", result );
                return result;
            }
        }
#endif
    }
#endif

    uint32_t dma_type = dma_max;

#if ISP_HAS_SC0
    if (streamType == V4L2_STREAM_TYPE_SC0)
        dma_type = dma_sc0;
#endif
#if ISP_HAS_SC1
    if (streamType == V4L2_STREAM_TYPE_SC1)
        dma_type = dma_sc1;
#endif
#if ISP_HAS_SC2
    if (streamType == V4L2_STREAM_TYPE_SC2)
        dma_type = dma_sc2;
#endif
#if ISP_HAS_SC3
    if (streamType == V4L2_STREAM_TYPE_CROP)
        dma_type = dma_sc3;
#endif

    uint32_t ret_val;
    uint32_t w, h, fps, idx, state;
    int result;

    w = *width;
    h = *height;

    uint32_t width_cur, height_cur, exposure_cur, wdr_mode_cur, fps_cur;
    wdr_mode_cur = 0;
    exposure_cur = 0;
    fps_cur = 0;
    if ( dma_type == dma_sc3 ) {
        acamera_command( ctx_id, TSENSOR, SENSOR_STREAMING, ON, COMMAND_GET, &state );
        acamera_command( ctx_id, TSENSOR, SENSOR_WIDTH, 0, COMMAND_GET, &width_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_HEIGHT, 0, COMMAND_GET, &height_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_EXPOSURES, 0, COMMAND_GET, &exposure_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_WDR_MODE, 0, COMMAND_GET, &wdr_mode_cur );
        acamera_command( ctx_id, TSENSOR, SENSOR_FPS, 0, COMMAND_GET, &fps_cur );
        if (state != ON) {
        LOG( LOG_DEBUG, "target (width = %d, height = %d, fps = %d) current (w=%d h=%d exposure_cur = %d wdr_mode_cur = %d, fps = %d)",
             w, h, custom_fps[ctx_id], width_cur, height_cur, exposure_cur, wdr_mode_cur, fps_cur / 256);

            if ( width_cur != w || height_cur != h || exposure_cur != custom_exp[ctx_id] || wdr_mode_cur != custom_wdr_mode[ctx_id] || fps_cur / 256 != custom_fps[ctx_id]) {

                idx = fw_intf_find_proper_present_idx(ctx_id, sensor_info, w, h, &fps);

                /* set sensor resolution preset */
                LOG( LOG_CRIT, "Setting new resolution : width = %d, height = %d (preset idx = %d, fps = %d)", w, h, idx, fps / 256 );
                result = acamera_command( ctx_id, TSENSOR, SENSOR_PRESET, idx, COMMAND_SET, &ret_val );
                *( (char *)&sensor_info->preset_cur ) = idx;
                if ( result ) {
                    LOG( LOG_CRIT, "Failed to set preset to %u, ret_value: %d.", idx, result );
                    return -EINVAL;
                }
            } else {
                LOG(LOG_ERR, "target resolution equal current resolution");
                acamera_command( ctx_id, TSENSOR, SENSOR_PRESET, 0, COMMAND_GET, &idx );
                LOG( LOG_CRIT, "Leaving same sensor settings resolution : width = %d, height = %d (preset idx = %d)", w, h, idx );
            }
        }
    }

    //check if we need to change sensor preset
    acamera_command( ctx_id, TSENSOR, SENSOR_WIDTH, 0, COMMAND_GET, &width_cur );
    acamera_command( ctx_id, TSENSOR, SENSOR_HEIGHT, 0, COMMAND_GET, &height_cur );
    LOG( LOG_CRIT, "sc:%d target (width = %d, height = %d) input (w=%d h=%d)", dma_type, w, h, width_cur, height_cur );

    acamera_command(ctx_id, TAML_SCALER, SCALER_WIDTH, w | (dma_type << 16), COMMAND_SET, &ret_val );
    acamera_command(ctx_id,  TAML_SCALER, SCALER_HEIGHT, h | (dma_type << 16), COMMAND_SET, &ret_val );
    acamera_command( ctx_id, TAML_SCALER, SCALER_SRC_WIDTH, width_cur | (dma_type << 16), COMMAND_SET, &ret_val );
    acamera_command( ctx_id, TAML_SCALER, SCALER_SRC_HEIGHT, height_cur | (dma_type << 16), COMMAND_SET, &ret_val );

    return 0;
}

int fw_intf_stream_set_output_format( uint32_t ctx_id, isp_v4l2_stream_type_t streamType, uint32_t format )
{

#if defined( TIMAGE ) && defined( FR_FORMAT_BASE_PLANE_ID )
    uint32_t value;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    switch ( format ) {
#ifdef DMA_FORMAT_RGB32
    case V4L2_PIX_FMT_RGB32:
        value = RGB32;
        break;
#endif
#ifdef DMA_FORMAT_NV12_Y
    case V4L2_PIX_FMT_NV12:
        value = NV12_YUV;
        break;
    case V4L2_PIX_FMT_GREY:
        value = NV12_GREY;
        break;
    case V4L2_PIX_FMT_NV21:
        value = NV12_YVU;
        break;
#endif
#ifdef DMA_FORMAT_A2R10G10B10
    case ISP_V4L2_PIX_FMT_ARGB2101010:
        value = A2R10G10B10;
        break;
#endif
#ifdef DMA_FORMAT_RGB24
    case V4L2_PIX_FMT_RGB24:
        value = RGB24;
        break;
#endif
#ifdef DMA_FORMAT_AYUV
    case V4L2_PIX_FMT_YUV444:
        value = AYUV;
        break;
#endif
#ifdef DMA_FORMAT_YUY2
    case V4L2_PIX_FMT_YUYV:
        value = YUY2;
        break;
#endif
#ifdef DMA_FORMAT_UYVY
    case V4L2_PIX_FMT_UYVY:
        value = UYVY;
        break;
#endif
#ifdef DMA_FORMAT_RAW16
    case V4L2_PIX_FMT_SBGGR16:
        value = RAW16;
        break;
#endif
#ifdef DMA_FORMAT_DISABLE
    case ISP_V4L2_PIX_FMT_NULL:
        value = DMA_DISABLE;
        break;
#endif
    case ISP_V4L2_PIX_FMT_META:
        LOG( LOG_INFO, "Meta format 0x%x doesn't need to be set to firmware", format );
        return 0;
        break;
    default:
        LOG( LOG_ERR, "Requested format 0x%x is not supported by firmware !", format );
        return -1;
        break;
    }


    uint8_t result;
    uint32_t ret_val;

    result = acamera_command( ctx_id, TIMAGE, FR_FORMAT_BASE_PLANE_ID, RGB24, COMMAND_SET, &ret_val );

#if ISP_HAS_DS1
    if ( streamType == V4L2_STREAM_TYPE_DS1 ) {

        result = acamera_command( ctx_id, TIMAGE, DS1_FORMAT_BASE_PLANE_ID, value, COMMAND_SET, &ret_val );
        if ( result ) {
            LOG( LOG_ERR, "TIMAGE - DS1_FORMAT_BASE_PLANE_ID failed (value = 0x%x, result = %d)", value, result );
        }
    }
#endif

    uint32_t dma_type = dma_max;

#if ISP_HAS_SC0
    if (streamType == V4L2_STREAM_TYPE_SC0)
        dma_type = dma_sc0;
#endif
#if ISP_HAS_SC1
    if (streamType == V4L2_STREAM_TYPE_SC1)
        dma_type = dma_sc1;
#endif
#if ISP_HAS_SC2
    if (streamType == V4L2_STREAM_TYPE_SC2)
        dma_type = dma_sc2;
#endif
#if ISP_HAS_SC3
    if (streamType == V4L2_STREAM_TYPE_CROP)
        dma_type = dma_sc3;
#endif

    if ( streamType < V4L2_STREAM_TYPE_MAX ) {
        if ( value == YUY2 ) {
            result = acamera_command( ctx_id, TSENSOR, SENSOR_BAYER_PATTERN, 0, COMMAND_GET, &ret_val );
            if ( ret_val == BAYER_YUY2 )
                value = RAW_YUY2;
        }
    }

    result = acamera_command( ctx_id, TAML_SCALER, SCALER_OUTPUT_MODE, value | (dma_type << 16), COMMAND_SET, &ret_val );
    LOG( LOG_ERR, "set format for stream %d to %d (0x%x)", streamType, value, format );
    if ( result ) {
        LOG( LOG_ERR, "TIMAGE - SC0_FORMAT_BASE_PLANE_ID failed (value = 0x%x, result = %d)", value, result );
    }
#else
    LOG( LOG_ERR, "cannot find proper API for fr base mode ID" );
#endif

    return 0;
}

static int fw_intf_set_fr_fps(uint32_t ctx_id, uint32_t fps)
{
    uint32_t cur_fps = 0;

    acamera_command(ctx_id, TSENSOR, SENSOR_VMAX_FPS, 0, COMMAND_GET, &cur_fps);
    if (cur_fps == 0) {
        LOG(LOG_ERR, "Error input param\n");
        return -1;
    }

    //cur_fps = cur_fps / 256;

    acamera_api_set_fps(ctx_id, dma_fr, cur_fps, fps);

    return 0;
}

static int fw_intf_set_sensor_dcam_mode(uint32_t ctx_id, uint32_t val)
{
    uint32_t mode = val;
    uint32_t ret_val;
    acamera_command(ctx_id, TSENSOR, SENSOR_DCAM, mode, COMMAND_SET, &ret_val);
    if (mode <= 0) {
        LOG(LOG_ERR, "Error input param\n");
        return -1;
    }
    return 0;
}

static int fw_intf_set_sensor_testpattern(uint32_t ctx_id, uint32_t val)
{
    uint32_t mode = val;
    uint32_t ret_val;
    acamera_command(ctx_id, TSENSOR, SENSOR_TESTPATTERN, mode, COMMAND_SET, &ret_val);
    if (mode <= 0) {
        LOG(LOG_ERR, "Error input param\n");
        return -1;
    }
    return 0;
}

static int fw_intf_set_sensor_ir_cut_set(uint32_t ctx_id, uint32_t ctrl_val)
{
    uint32_t ir_cut_state = ctrl_val;
    acamera_command(ctx_id, TSENSOR, SENSOR_IR_CUT, 0, COMMAND_SET, &ir_cut_state);
    return 0;
}

static int fw_intf_set_ds1_fps(uint32_t ctx_id, uint32_t fps)
{
    uint32_t cur_fps = 0;

    acamera_command(ctx_id, TSENSOR, SENSOR_VMAX_FPS, 0, COMMAND_GET, &cur_fps);
    if (cur_fps == 0) {
        LOG(LOG_ERR, "Error input param\n");
        return -1;
    }

    //cur_fps = cur_fps / 256;

    acamera_api_set_fps(ctx_id, dma_ds1, cur_fps, fps);

    return 0;
}


static int fw_intf_set_ae_zone_weight(uint32_t ctx_id, uint32_t * ctrl_val)
{
    acamera_command(ctx_id, TALGORITHMS, AE_ZONE_WEIGHT, 0, COMMAND_SET, ctrl_val);

    return 0;
}

static int fw_intf_get_ae_zone_weight( uint32_t ctx_id, int *ret_val )
{
    acamera_command(ctx_id, TALGORITHMS, AE_ZONE_WEIGHT, 0, COMMAND_GET, (uint32_t *)ret_val);

    return 0;
}

static int fw_intf_set_awb_zone_weight(uint32_t ctx_id, uint32_t * ctrl_val)
{
    acamera_command(ctx_id, TALGORITHMS, AWB_ZONE_WEIGHT, 0, COMMAND_SET, ctrl_val);

    return 0;
}

static int fw_intf_get_awb_zone_weight( uint32_t ctx_id, int *ret_val )
{
    acamera_command(ctx_id, TALGORITHMS, AWB_ZONE_WEIGHT, 0, COMMAND_GET, (uint32_t *)ret_val);

    return 0;
}

static int fw_intf_set_sensor_integration_time(uint32_t ctx_id, uint32_t ctrl_val)
{
    uint32_t manual_sensor_integration_time = ctrl_val;
    acamera_command(ctx_id, TSYSTEM, SYSTEM_INTEGRATION_TIME, manual_sensor_integration_time, COMMAND_SET, &ctrl_val );

    return 0;
}

static int fw_intf_set_sensor_analog_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    uint32_t manual_sensor_analog_gain = ctrl_val;
    acamera_command(ctx_id, TSYSTEM, SYSTEM_SENSOR_ANALOG_GAIN, manual_sensor_analog_gain, COMMAND_SET, &ctrl_val );

    return 0;
}

static int fw_intf_set_isp_digital_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    uint32_t manual_isp_digital_gain = ctrl_val;
    acamera_command(ctx_id, TSYSTEM, SYSTEM_ISP_DIGITAL_GAIN, manual_isp_digital_gain, COMMAND_SET, &ctrl_val );

    return 0;
}

static int fw_intf_set_stop_sensor_update(uint32_t ctx_id, uint32_t ctrl_val)
{
    uint32_t stop_sensor_update = ctrl_val;
    LOG(LOG_ERR, "stop_sensor_update = %d\n", stop_sensor_update);
    acamera_command(ctx_id, TSYSTEM, SYSTEM_FREEZE_FIRMWARE, stop_sensor_update, COMMAND_SET, &ctrl_val);
    return 0;
}

static int fw_intf_set_sensor_digital_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    uint32_t manual_sensor_digital_gain = ctrl_val;
    acamera_command(ctx_id, TSYSTEM, SYSTEM_SENSOR_DIGITAL_GAIN, manual_sensor_digital_gain, COMMAND_SET, &ctrl_val );

    return 0;
}

static int fw_intf_set_awb_red_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    uint32_t awb_red_gain = ctrl_val;
    acamera_command(ctx_id, TSYSTEM, SYSTEM_AWB_RED_GAIN, awb_red_gain, COMMAND_SET, &ctrl_val );

    return 0;
}

static int fw_intf_set_awb_blue_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    uint32_t awb_blue_gain = ctrl_val;
    acamera_command(ctx_id, TSYSTEM, SYSTEM_AWB_BLUE_GAIN, awb_blue_gain, COMMAND_SET, &ctrl_val );

    return 0;
}

/* ----------------------------------------------------------------
 * Internal handler for control interface functions
 */
static bool isp_fw_do_validate_control( uint32_t id )
{
    return 1;
}

static int isp_fw_do_set_test_pattern( uint32_t ctx_id, int enable )
{
#if defined( TSYSTEM ) && defined( TEST_PATTERN_ENABLE_ID )
    int result;
    uint32_t ret_val;

    LOG( LOG_CRIT, "test_pattern: %d.", enable );

    if ( enable < 0 )
        return -EIO;

    if ( !isp_started ) {
        LOG( LOG_CRIT, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = application_command( TSYSTEM, TEST_PATTERN_ENABLE_ID, enable ? ON : OFF, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_CRIT, "Failed to set TEST_PATTERN_ENABLE_ID to %u, ret_value: %d.", enable, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_test_pattern_type( uint32_t ctx_id, int pattern_type )
{
#if defined( TSYSTEM ) && defined( TEST_PATTERN_MODE_ID )
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSYSTEM, TEST_PATTERN_MODE_ID, pattern_type, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TEST_PATTERN_MODE_ID to %d, ret_value: %d.", pattern_type, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_awb_mode_id( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( AWB_MODE_ID )
    uint32_t awb_mode_id_val;

    switch ( val ) {
    case ISP_V4L2_AWB_MODE_AUTO:
        awb_mode_id_val = AWB_AUTO;
        break;
    case ISP_V4L2_AWB_MODE_MANUAL:
        awb_mode_id_val = AWB_MANUAL;
        break;
    case ISP_V4L2_AWB_MODE_DAY_LIGHT:
        awb_mode_id_val = AWB_DAY_LIGHT;
        break;
    case ISP_V4L2_AWB_MODE_CLOUDY:
        awb_mode_id_val = AWB_CLOUDY;
        break;
    case ISP_V4L2_AWB_MODE_INCANDESCENT:
        awb_mode_id_val = AWB_INCANDESCENT;
        break;
    case ISP_V4L2_AWB_MODE_FLUORESCENT:
        awb_mode_id_val = AWB_FLOURESCENT;
        break;
    case ISP_V4L2_AWB_MODE_TWILIGHT:
        awb_mode_id_val = AWB_TWILIGHT;
        break;
    case ISP_V4L2_AWB_MODE_SHADE:
        awb_mode_id_val = AWB_SHADE;
        break;
    case ISP_V4L2_AWB_MODE_WARM_FLUORESCENT:
        awb_mode_id_val = AWB_WARM_FLOURESCENT;
        break;
    default:
        LOG( LOG_ERR, "unknown val: %d", val );
        return -1;
    };

    rc = isp_fw_do_set_cmd( ctx_id, TALGORITHMS, AWB_MODE_ID, awb_mode_id_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set AWB_MODE_ID val: %d, rc: %d", val, rc );
        return rc;
    }
#endif

    return rc;
}

static int isp_fw_do_set_noise_reduction_mode_id( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( NOISE_REDUCTION_MODE_ID )
    uint32_t nr_mode_id_val;

    switch ( val ) {
    case ISP_V4L2_NOISE_REDUCTION_MODE_OFF:
        nr_mode_id_val = NOISE_REDUCTION_OFF;
        break;
    case ISP_V4L2_NOISE_REDUCTION_MODE_ON:
        nr_mode_id_val = NOISE_REDUCTION_ON;
        break;
    default:
        LOG( LOG_ERR, "unknown val: %d", val );
        return -1;
    };

    rc = isp_fw_do_set_cmd( ctx_id, TALGORITHMS, NOISE_REDUCTION_MODE_ID, nr_mode_id_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set NOISE_REDUCTION_MODE_ID val: %d, rc: %d", val, rc );
        return rc;
    }
#endif

    return rc;
}

static int isp_fw_do_set_af_refocus( uint32_t ctx_id, int val )
{
#if defined( TALGORITHMS ) && defined( AF_MODE_ID )
    int result;
    u32 ret_val;

    result = acamera_command( ctx_id, TALGORITHMS, AF_MODE_ID, AF_AUTO_SINGLE, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AF_MODE_ID to AF_AUTO_SINGLE, ret_value: %u.", ret_val );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_af_roi( uint32_t ctx_id, int val )
{
#if defined( TALGORITHMS ) && defined( AF_ROI_ID )
    int result;
    u32 ret_val;

    result = acamera_command( ctx_id, TALGORITHMS, AF_ROI_ID, (uint32_t)val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AF_MODE_ID to AF_AUTO_SINGLE, ret_value: %u.", ret_val );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_ae_roi( uint32_t ctx_id, int val )
{
#if defined( TALGORITHMS ) && defined( AE_ROI_ID )
    int result;
    u32 ret_val;

    result = acamera_command( ctx_id, TALGORITHMS, AE_ROI_ID, (uint32_t)val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AE_ROI_ID, ret_value: %u.", ret_val );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_brightness( uint32_t ctx_id, int brightness )
{
#if defined( TSCENE_MODES ) && defined( BRIGHTNESS_STRENGTH_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, BRIGHTNESS_STRENGTH_ID, brightness, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set BRIGHTNESS_STRENGTH_ID to %d, ret_value: %d.", brightness, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_contrast( uint32_t ctx_id, int contrast )
{
#if defined( TSCENE_MODES ) && defined( CONTRAST_STRENGTH_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, CONTRAST_STRENGTH_ID, contrast, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set CONTRAST_STRENGTH_ID to %d, ret_value: %d.", contrast, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_saturation( uint32_t ctx_id, int saturation )
{
#if defined( TSCENE_MODES ) && defined( SATURATION_STRENGTH_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, SATURATION_STRENGTH_ID, saturation, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SATURATION_STRENGTH_ID to %d, ret_value: %d.", saturation, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_hue( uint32_t ctx_id, int hue )
{
#if defined( TSCENE_MODES ) && defined( HUE_THETA_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, HUE_THETA_ID, hue, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set HUE_THETA_ID to %d, ret_value: %d.", hue, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_sharpness( uint32_t ctx_id, int sharpness )
{
#if defined( TSCENE_MODES ) && defined( SHARPENING_STRENGTH_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, SHARPENING_STRENGTH_ID, sharpness, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SHARPENING_STRENGTH_ID to %d, ret_value: %d.", sharpness, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_color_fx( uint32_t ctx_id, int idx )
{
#if defined( TSCENE_MODES ) && defined( COLOR_MODE_ID )
    int result;
    uint32_t ret_val;
    int color_idx;

    switch ( idx ) {
    case V4L2_COLORFX_NONE:
        color_idx = NORMAL;
        break;
    case V4L2_COLORFX_BW:
        color_idx = BLACK_AND_WHITE;
        break;
    case V4L2_COLORFX_SEPIA:
        color_idx = SEPIA;
        break;
    case V4L2_COLORFX_NEGATIVE:
        color_idx = NEGATIVE;
        break;
    case V4L2_COLORFX_VIVID:
        color_idx = VIVID;
        break;
    default:
        return -EINVAL;
        break;
    }

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, COLOR_MODE_ID, color_idx, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_ISP_DIGITAL_GAIN to %d, ret_value: %d.", color_idx, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_hflip( uint32_t ctx_id, bool enable )
{
#if defined( TIMAGE ) && defined( ORIENTATION_HFLIP_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TIMAGE, ORIENTATION_HFLIP_ID, enable ? ENABLE : DISABLE, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set ORIENTATION_HFLIP_ID to %d, ret_value: %d.", enable, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_vflip( uint32_t ctx_id, bool enable )
{
#if defined( TIMAGE ) && defined( ORIENTATION_VFLIP_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TIMAGE, ORIENTATION_VFLIP_ID, enable ? ENABLE : DISABLE, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set ORIENTATION_VFLIP_ID to %d, ret_value: %d.", enable, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_manual_gain( uint32_t ctx_id, bool enable )
{
#if defined( TALGORITHMS ) && defined( AE_MODE_ID )
    int result;
    uint32_t mode = 0;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AE_MODE_ID, 0, COMMAND_GET, &ret_val );
    if ( enable ) {
        if ( ret_val == AE_AUTO ) {
            mode = AE_MANUAL_GAIN;
        } else if ( ret_val == AE_MANUAL_EXPOSURE_TIME ) {
            mode = AE_FULL_MANUAL;
        } else {
            LOG( LOG_DEBUG, "Manual gain is already enabled." );
            return 0;
        }
    } else {
        if ( ret_val == AE_MANUAL_GAIN ) {
            mode = AE_AUTO;
        } else if ( ret_val == AE_FULL_MANUAL ) {
            mode = AE_MANUAL_EXPOSURE_TIME;
        } else {
            LOG( LOG_DEBUG, "Manual gain is already disabled." );
            return 0;
        }
    }

    result = acamera_command( ctx_id, TALGORITHMS, AE_MODE_ID, mode, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AE_MODE_ID to %u, ret_value: %d.", mode, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_gain( uint32_t ctx_id, int gain )
{
#if defined( TALGORITHMS ) && defined( AE_GAIN_ID )
    int result;
    int gain_frac;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AE_MODE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val != AE_FULL_MANUAL && ret_val != AE_MANUAL_GAIN ) {
        LOG( LOG_ERR, "Cannot set gain while AE_MODE is %d", ret_val );
        return 0;
    }

    gain_frac = ( gain / 100 ) * 256;
    gain_frac += ( gain % 100 ) * 256 / 100;

    result = acamera_command( ctx_id, TALGORITHMS, AE_GAIN_ID, gain_frac, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AE_GAIN_ID to %d, ret_value: %d.", gain, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_exposure_auto( uint32_t ctx_id, int enable )
{
#if defined( TALGORITHMS ) && defined( AE_MODE_ID )
    int result;
    uint32_t mode = 0;
    uint32_t ret_val;
    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AE_MODE_ID, 0, COMMAND_GET, &ret_val );
    switch ( enable ) {
    case true:
        if ( ret_val == AE_AUTO ) {
            mode = AE_MANUAL_EXPOSURE_TIME;
        } else if ( ret_val == AE_MANUAL_GAIN ) {
            mode = AE_FULL_MANUAL;
        } else {
            LOG( LOG_DEBUG, "Manual exposure is already enabled." );
            return 0;
        }
        break;
    case false:
        if ( ret_val == AE_MANUAL_EXPOSURE_TIME ) {
            mode = AE_AUTO;
        } else if ( ret_val == AE_FULL_MANUAL ) {
            mode = AE_MANUAL_GAIN;
        } else {
            LOG( LOG_DEBUG, "Manual exposure is already disabled." );
            return 0;
        }
        break;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AE_MODE_ID, mode, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AE_MODE_ID to %u, ret_value: %d.", mode, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_manual_exposure( uint32_t ctx_id, int enable )
{
#if defined( TALGORITHMS ) && defined( AE_MODE_ID )
    int result_integration_time, result_sensor_analog_gain, result_sensor_digital_gain, result_isp_digital_gain;
    uint32_t ret_val;

    LOG( LOG_ERR, "manual exposure enable: %d.", enable );

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result_integration_time = acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_INTEGRATION_TIME, enable, COMMAND_SET, &ret_val );
    if ( result_integration_time ) {
        LOG( LOG_ERR, "Failed to set manual_integration_time to manual mode, ret_value: %d", result_integration_time );
        return ( result_integration_time );
    }

    result_sensor_analog_gain = acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_SENSOR_ANALOG_GAIN, enable, COMMAND_SET, &ret_val );
    if ( result_sensor_analog_gain ) {
        LOG( LOG_ERR, "Failed to set manual_sensor_analog_gain to manual mode, ret_value: %d", result_sensor_analog_gain );
        return ( result_sensor_analog_gain );
    }

    result_sensor_digital_gain = acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN, enable, COMMAND_SET, &ret_val );
    if ( result_sensor_digital_gain ) {
        LOG( LOG_ERR, "Failed to set manual_sensor_digital_gain to manual mode, ret_value: %d", result_sensor_digital_gain );
        return ( result_sensor_digital_gain );
    }

    result_isp_digital_gain = acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_ISP_DIGITAL_GAIN, enable, COMMAND_SET, &ret_val );
    if ( result_isp_digital_gain ) {
        LOG( LOG_ERR, "Failed to set manual_isp_digital_gain to manual mode, ret_value: %d", result_isp_digital_gain );
        return ( result_isp_digital_gain );
    }

#endif

    return 0;
}

/* set exposure in us unit */
static int isp_fw_do_set_exposure( uint32_t ctx_id, int exp )
{
#if defined( TALGORITHMS ) && defined( AE_EXPOSURE_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AE_EXPOSURE_ID, exp, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AE_EXPOSURE_ID to %d, ret_value: %d.", exp, result );
        return result;
    }
#endif
    return 0;
}

static int isp_fw_do_set_variable_frame_rate( uint32_t ctx_id, int enable )
{
    // SYSTEM_EXPOSURE_PRIORITY ??
    return 0;
}

static int isp_fw_do_set_white_balance_mode( uint32_t ctx_id, int wb_mode )
{
#if defined( TALGORITHMS ) && defined( AWB_MODE_ID )
#if defined( ISP_HAS_AWB_MESH_FSM ) || defined( ISP_HAS_AWB_MESH_NBP_FSM ) || defined( ISP_HAS_AWB_MANUAL_FSM )
    static int32_t last_wb_request = AWB_DAY_LIGHT;
#else
    static int32_t last_wb_request = AWB_MANUAL;
#endif
    int result;
    uint32_t mode = 0;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AWB_MODE_ID, 0, COMMAND_GET, &ret_val );
    LOG( LOG_CRIT, "AWB_MODE_ID = %d", ret_val );
    switch ( wb_mode ) {
    case AWB_MANUAL:
        /* we set the last mode instead of MANUAL */
        mode = last_wb_request;
        break;
    case AWB_AUTO:
        /* we set the last mode instead of MANUAL */
        if ( ret_val != AWB_AUTO ) {
            mode = wb_mode;
        } else {
            LOG( LOG_DEBUG, "Auto WB is already enabled." );
            return 0;
        }
        break;
#if defined( ISP_HAS_AWB_MESH_FSM ) || defined( ISP_HAS_AWB_MESH_NBP_FSM ) || defined( ISP_HAS_AWB_MANUAL_FSM )
    case AWB_DAY_LIGHT:
    case AWB_CLOUDY:
    case AWB_INCANDESCENT:
    case AWB_FLOURESCENT:
    case AWB_TWILIGHT:
    case AWB_SHADE:
    case AWB_WARM_FLOURESCENT:
        if ( ret_val != AWB_AUTO ) {
            mode = wb_mode;
        } else {
            /* wb mode is not updated when it's in auto mode */
            LOG( LOG_DEBUG, "Auto WB is enabled, remembering mode." );
            last_wb_request = wb_mode;
            return 0;
        }
        break;
#endif
    default:
        return -EINVAL;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AWB_MODE_ID, mode, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AWB_MODE_ID to %u, ret_value: %d.", mode, result );
        return result;
    }
#endif
    return 0;
}

static int isp_fw_do_set_focus_auto( uint32_t ctx_id, int enable )
{
#if defined( TALGORITHMS ) && defined( AF_MODE_ID )
    int result;
    uint32_t mode = 0;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AF_MODE_ID, 0, COMMAND_GET, &ret_val );
    switch ( enable ) {
    case 1:
        /* we set the last mode instead of MANUAL */
        if ( ret_val != AF_AUTO_CONTINUOUS ) {
            mode = AF_AUTO_CONTINUOUS;
        } else {
            LOG( LOG_DEBUG, "Auto focus is already enabled." );
            return 0;
        }
        break;
    case 0:
        if ( ret_val != AF_MANUAL ) {
            mode = AF_MANUAL;
        } else {
            /* wb mode is not updated when it's in auto mode */
            LOG( LOG_DEBUG, "Auto WB is enabled, remembering mode." );
            return 0;
        }
        break;
    default:
        return -EINVAL;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AF_MODE_ID, mode, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AF_MODE_ID to %u, ret_value: %d.", mode, result );
        return result;
    }
#endif

    return 0;
}

static int isp_fw_do_set_focus( uint32_t ctx_id, int focus )
{
#if defined( TALGORITHMS ) && defined( AF_MANUAL_CONTROL_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TALGORITHMS, AF_MANUAL_CONTROL_ID, focus, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AF_MANUAL_CONTROL_ID to %d, ret_value: %d.", focus, result );
        return result;
    }
#endif
    return 0;
}

static int isp_fw_do_set_ae_scene( uint32_t ctx_id, int scene )
{
#if defined( TSCENE_MODES ) && defined( AE_SCENE_MODE_ID )
    int result;
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, AE_SCENE_MODE_ID, scene, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set AE_SCENE_MODE_ID to %d, ret_value: %d.", scene, result );
        return result;
    }
#endif
    return 0;
}

static int isp_fw_do_set_max_integration_time( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;

    if ( val < 0 )
        return -EIO;

    if ( !isp_started ) {
        LOG( LOG_NOTICE, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSYSTEM, SYSTEM_MAX_INTEGRATION_TIME, val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set max_integration_time to %u, ret_value: %d.", val, result );
        return result;
    }
    return 0;
}

static int isp_fw_do_set_snr_manual( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, SNR_MANUAL_ID, val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SNR_MANUAL_ID to %d, ret_value: %d.", val, result );
        return result;
    }

    return 0;

}

static int isp_fw_do_set_snr_strength( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, SNR_STRENGTH_ID, val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SNR_OFFSET_ID to %d, ret_value: %d.", val, result );
        return result;
    }

    return 0;

}

static int isp_fw_do_set_tnr_manual( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_MANUAL_ID, val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_MANUAL_ID to %d, ret_value: %d.", val, result );
        return result;
    }

    return 0;

}

static int isp_fw_do_set_tnr_offset( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_OFFSET_ID, val, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_OFFSET_ID to %d, ret_value: %d.", val, result );
        return result;
    }

    return 0;

}

static int isp_fw_do_set_temper_mode( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;
    uint32_t temper_mode;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    if ( val == 1 )
        temper_mode = TEMPER2_MODE;
    else
        temper_mode = TEMPER3_MODE;

    result = acamera_command( ctx_id, TSYSTEM, TEMPER_MODE_ID, temper_mode, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TEMPER_MODE_ID to %d, ret_value: %d.", temper_mode, result );
        return result;
    }

    return 0;

}

static int isp_fw_do_set_sensor_dynamic_mode( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;
    uint32_t preset_mode = val;

    result = acamera_command( ctx_id, TSENSOR, SENSOR_WDRMODE_ID, preset_mode, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SENSOR_MODE_SWITCH to %d, ret_value: %d.", preset_mode, result );
        return result;
    }

    return 0;
}

static int isp_fw_do_set_sensor_antiflicker( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;
    uint32_t fps = val;

    result = acamera_command( ctx_id, TSENSOR, SENSOR_ANTIFLICKER_ID, fps, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SENSOR_ANTIFLICKER to %d, ret_value: %d.", fps, result );
        return result;
    }

    return 0;
}

static int isp_fw_do_set_defog_mode( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;
    uint32_t mode = 0;

    switch (val) {
    case 0:
        mode = DEFOG_DISABLE;
    break;
    case 1:
        mode = DEFOG_ONLY;
    break;
    case 2:
        mode = DEFOG_BLEND;
    break;
    default:
        mode = DEFOG_DISABLE;
    break;
    }

    result = acamera_command( ctx_id, TALGORITHMS, DEFOG_MODE_ID, mode, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SENSOR_MODE_SWITCH to %d, ret_value: %d.", mode, result );
        return result;
    }

    return 0;
}

static int isp_fw_do_set_defog_ratio( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;
    uint32_t ratio = val;

    result = acamera_command( ctx_id, TALGORITHMS, DEFOG_RATIO_DELTA, ratio, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SENSOR_MODE_SWITCH to %d, ret_value: %d.", ratio, result );
        return result;
    }

    return 0;
}

static int isp_fw_do_set_calibration( uint32_t ctx_id, int val )
{
    int result;
    uint32_t ret_val;
    uint32_t value = val;

    result = acamera_command( ctx_id, TSYSTEM, CALIBRATION_CUSTOMER, value, COMMAND_SET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set CALIBRATION_CUSTOMER to %d, ret_value: %d.", value, result );
        return result;
    }

    return 0;
}

static int isp_fw_do_get_gain( uint32_t ctx_id, uint32_t * gain )
{
#if defined( TALGORITHMS ) && defined( AE_GAIN_ID )
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }
/*
gain_frac = ( gain / 100 ) * 256;
gain_frac += ( gain % 100 ) * 256 / 100;

*/
    acamera_command( ctx_id, TALGORITHMS, AE_GAIN_ID, 0, COMMAND_GET, &ret_val );
    *gain = (ret_val/256*100)+(ret_val%256*100/256);

#endif

    return 0;
}



/* get exposure in us unit */
static int isp_fw_do_get_exposure( uint32_t ctx_id, uint32_t *exp )
{
#if defined( TALGORITHMS ) && defined( AE_EXPOSURE_ID )
    uint32_t ret_val;

    /* some controls(such brightness) will call acamera_command()
     * before isp_fw initialed, so we need to check.
     */
    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    acamera_command( ctx_id, TALGORITHMS, AE_EXPOSURE_ID, 0, COMMAND_GET, &ret_val );
    *exp = ret_val;
#endif
    return 0;
}

static int isp_fw_do_get_sensor_dynamic_mode( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_NOTICE, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSENSOR, SENSOR_WDRMODE_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to get SENSOR_MODE_SWITCH, ret_value: %d.", result );
        return result;
    }

    return ret_val;
}

static int isp_fw_do_get_sensor_antiflicker( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_NOTICE, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSENSOR, SENSOR_ANTIFLICKER_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to get SENSOR_ANTIFLICKER, ret_value: %d.", result );
        return result;
    }

    return ret_val;
}

static int isp_fw_do_get_snr_manual( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, SNR_MANUAL_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SNR_MANUAL_ID, ret_value: %d.", result );
        return result;
    }

    return ret_val;
}

static int isp_fw_do_get_snr_strength( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, SNR_STRENGTH_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set SNR_OFFSET_ID, ret_value: %d.", result );
        return result;
    }

    return ret_val;

}

static int isp_fw_do_get_tnr_manual( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_MANUAL_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_MANUAL_ID, ret_value: %d.", result );
        return result;
    }

    return ret_val;

}

static int isp_fw_do_get_tnr_offset( uint32_t ctx_id )
{
    int result;
    uint32_t ret_val;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    result = acamera_command( ctx_id, TSCENE_MODES, TNR_OFFSET_ID, 0, COMMAND_GET, &ret_val );
    if ( result ) {
        LOG( LOG_ERR, "Failed to set TNR_OFFSET_ID, ret_value: %d.", result );
        return result;
    }

    return ret_val;

}

static int isp_fw_do_set_cmd( uint32_t ctx_id, uint8_t command_type, uint8_t command, uint32_t value )
{
    int rc = 0;
    uint32_t ret_val = 0;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    rc = acamera_command( ctx_id, command_type, command, value, COMMAND_SET, &ret_val );
    return rc;
}

static int isp_fw_do_get_cmd( uint32_t ctx_id, uint8_t command_type, uint8_t command, uint32_t *ret_val )
{
    int rc = 0;

    if ( !isp_started ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return -EBUSY;
    }

    rc = acamera_command( ctx_id, command_type, command, 0, COMMAND_GET, ret_val );
    return rc;
}

/* ----------------------------------------------------------------
 * fw_interface config interface
 */
bool fw_intf_validate_control( uint32_t id )
{
    return isp_fw_do_validate_control( id );
}

int fw_intf_set_test_pattern( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_test_pattern( ctx_id, val );
}

int fw_intf_set_test_pattern_type( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_test_pattern_type( ctx_id, val );
}

int fw_intf_set_awb_mode_id( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_awb_mode_id( ctx_id, val );
}

int fw_intf_set_noise_reduction_mode_id( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_noise_reduction_mode_id( ctx_id, val );
}

int fw_intf_set_af_refocus( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_af_refocus( ctx_id, val );
}

int fw_intf_set_af_roi( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_af_roi( ctx_id, val );
}

int fw_intf_set_ae_roi( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_ae_roi( ctx_id, val );
}

int fw_intf_set_brightness( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_brightness( ctx_id, val );
}

int fw_intf_set_contrast( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_contrast( ctx_id, val );
}

int fw_intf_set_saturation( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_saturation( ctx_id, val );
}

int fw_intf_set_hue( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_hue( ctx_id, val );
}

int fw_intf_set_sharpness( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_sharpness( ctx_id, val );
}

int fw_intf_set_color_fx( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_color_fx( ctx_id, val );
}

int fw_intf_set_hflip( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_hflip( ctx_id, val ? 1 : 0 );
}

int fw_intf_set_vflip( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_vflip( ctx_id, val ? 1 : 0 );
}

int fw_intf_set_autogain( uint32_t ctx_id, int val )
{
    /* autogain enable: disable manual gain.
     * autogain disable: enable manual gain.
     */
    return isp_fw_do_set_manual_gain( ctx_id, val ? 0 : 1 );
}

int fw_intf_set_gain( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_gain( ctx_id, val );
}

int fw_intf_set_exposure_auto( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_exposure_auto( ctx_id, val );
}

int fw_intf_set_exposure( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_exposure( ctx_id, val );
}

int fw_intf_set_variable_frame_rate( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_variable_frame_rate( ctx_id, val );
}

int fw_intf_set_white_balance_auto( uint32_t ctx_id, int val )
{
#ifdef AWB_MODE_ID
    int mode;

    if ( val == true )
        mode = AWB_AUTO;
    else
        mode = AWB_MANUAL;

    return isp_fw_do_set_white_balance_mode( ctx_id, mode );
#endif

    // return SUCCESS for compatibility verfication issue
    return 0;
}

int fw_intf_set_white_balance( uint32_t ctx_id, int val )
{
    int mode = 0;

    switch ( val ) {
#ifdef AWB_MODE_ID
#if defined( ISP_HAS_AWB_MESH_FSM ) || defined( ISP_HAS_AWB_MESH_NBP_FSM ) || defined( ISP_HAS_AWB_MANUAL_FSM )
    case 8000:
        mode = AWB_SHADE;
        break;
    case 7000:
        mode = AWB_CLOUDY;
        break;
    case 6000:
    case 5000:
        mode = AWB_DAY_LIGHT;
        break;
    case 4000:
        mode = AWB_FLOURESCENT;
        break;
    case 3000:
        mode = AWB_WARM_FLOURESCENT;
        break;
    case 2000:
        mode = AWB_INCANDESCENT;
        break;
#endif
#endif
    default:
        // return SUCCESS for compatibility verfication issue
        return 0;
    }

    return isp_fw_do_set_white_balance_mode( ctx_id, mode );
}

int fw_intf_set_focus_auto( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_focus_auto( ctx_id, val );
}

int fw_intf_set_focus( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_focus( ctx_id, val );
}

int fw_intf_set_ae_scene( uint32_t ctx_id, int val )
{
    return isp_fw_do_set_ae_scene( ctx_id, val );
}

int fw_intf_set_output_fr_on_off( uint32_t ctx_id, uint32_t ctrl_val )
{
    return fw_intf_stream_set_output_format( ctx_id, V4L2_STREAM_TYPE_FR, ctrl_val );
}

int fw_intf_set_output_ds1_on_off( uint32_t ctx_id, uint32_t ctrl_val )
{
#if ISP_HAS_DS1
    return fw_intf_stream_set_output_format( ctx_id, V4L2_STREAM_TYPE_DS1, ctrl_val );
#else
    return 0;
#endif
}

int fw_intf_set_custom_sensor_wdr_mode(uint32_t ctx_id, uint32_t ctrl_val)
{
    custom_wdr_mode[ctx_id] = ctrl_val;
    return 0;
}

int fw_intf_set_custom_sensor_exposure(uint32_t ctx_id, uint32_t ctrl_val)
{
    custom_exp[ctx_id] = ctrl_val;
    return 0;
}

int fw_intf_set_custom_sensor_fps(uint32_t ctx_id, uint32_t ctrl_val)
{
    custom_fps[ctx_id] = ctrl_val;
    return 0;
}

int fw_intf_set_custom_snr_manual(uint32_t ctx_id, uint32_t ctrl_val)
{
    return isp_fw_do_set_snr_manual(ctx_id, ctrl_val);
}

int fw_intf_set_custom_snr_strength(uint32_t ctx_id, uint32_t ctrl_val)
{
    return isp_fw_do_set_snr_strength(ctx_id, ctrl_val);
}

int fw_intf_set_custom_tnr_manual(uint32_t ctx_id, uint32_t ctrl_val)
{
    return isp_fw_do_set_tnr_manual(ctx_id, ctrl_val);
}

int fw_intf_set_custom_tnr_offset(uint32_t ctx_id, uint32_t ctrl_val)
{
    return isp_fw_do_set_tnr_offset(ctx_id, ctrl_val);
}

int fw_intf_set_custom_dcam_mode(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    rtn = fw_intf_set_sensor_dcam_mode(ctx_id, ctrl_val);

    return rtn;

}
int fw_intf_set_custom_fr_fps(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    rtn = fw_intf_set_fr_fps(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_custom_ds1_fps(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    rtn = fw_intf_set_ds1_fps(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_custom_sensor_testpattern(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    rtn = fw_intf_set_sensor_testpattern(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_sensor_ir_cut(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    rtn = fw_intf_set_sensor_ir_cut_set(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_ae_zone_weight(uint32_t ctx_id, uint32_t* ctrl_val)
{
    int rtn = -1;

    rtn = fw_intf_set_ae_zone_weight(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_awb_zone_weight(uint32_t ctx_id, uint32_t* ctrl_val)
{
    int rtn = -1;

    rtn = fw_intf_set_awb_zone_weight(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_manual_exposure( uint32_t ctx_id, int val )
{
    int rtn = -1;

    if ( val == -1) {
       return 0;
    }

    rtn = isp_fw_do_set_manual_exposure( ctx_id, val );

    return rtn;
}

int fw_intf_set_customer_sensor_integration_time(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    if ( ctrl_val == -1) {
       return 0;
    }

    rtn = fw_intf_set_sensor_integration_time(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_sensor_analog_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    if ( ctrl_val == -1) {
       return 0;
    }

    rtn = fw_intf_set_sensor_analog_gain(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_isp_digital_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    if ( ctrl_val == -1) {
       return 0;
    }

    rtn = fw_intf_set_isp_digital_gain(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_stop_sensor_update(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    if ( ctrl_val == -1) {
       return 0;
    }

    rtn = fw_intf_set_stop_sensor_update(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_sensor_digital_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    if ( ctrl_val == -1) {
       return 0;
    }

    rtn = fw_intf_set_sensor_digital_gain(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_awb_red_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    if ( ctrl_val == -1) {
       return 0;
    }

    rtn = fw_intf_set_awb_red_gain(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_awb_blue_gain(uint32_t ctx_id, uint32_t ctrl_val)
{
    int rtn = -1;

    if ( ctrl_val == -1) {
       return 0;
    }

    rtn = fw_intf_set_awb_blue_gain(ctx_id, ctrl_val);

    return rtn;
}

int fw_intf_set_customer_max_integration_time(uint32_t ctx_id, uint32_t ctrl_val)
{
    if ( ctrl_val == -1) {
       return 0;
    }
    return isp_fw_do_set_max_integration_time(ctx_id, ctrl_val);
}

int fw_intf_set_customer_sensor_mode(uint32_t ctx_id, uint32_t ctrl_val)
{
    if ( ctrl_val < 0) {
       return 0;
    }

    return isp_fw_do_set_sensor_dynamic_mode(ctx_id, ctrl_val);
}

int fw_intf_set_customer_antiflicker(uint32_t ctx_id, uint32_t ctrl_val)
{
    int ret = 0;
    if ( ctrl_val < 0) {
       return 0;
    }

    ret = isp_fw_do_set_sensor_antiflicker(ctx_id, ctrl_val);
    if (!ret) {
        acamera_api_set_fps(ctx_id, dma_fr, (ctrl_val>30 ? ctrl_val/2:ctrl_val), 0);/*note: 0- only update c_fps*/
        acamera_api_set_fps(ctx_id, dma_ds1, (ctrl_val>30 ? ctrl_val/2:ctrl_val), 0);/*note: 0- only update c_fps*/
    }
    return ret;
}

int fw_intf_set_customer_defog_mode(uint32_t ctx_id, uint32_t ctrl_val)
{
    if ( ctrl_val > 2 ) {
       return -1;
    }

    return isp_fw_do_set_defog_mode(ctx_id, ctrl_val);
}

int fw_intf_set_customer_defog_ratio(uint32_t ctx_id, uint32_t ctrl_val)
{
    if ( ctrl_val > 4096 ) {
       return -1;
    }

    return isp_fw_do_set_defog_ratio(ctx_id, ctrl_val);
}

int fw_intf_set_customer_calibration(uint32_t ctx_id, uint32_t ctrl_val)
{
    if ( ctrl_val > 16 ) {
       return -1;
    }

    return isp_fw_do_set_calibration(ctx_id, ctrl_val);
}

int fw_intf_get_gain( uint32_t ctx_id, uint32_t * val )
{
    return isp_fw_do_get_gain( ctx_id, val );
}

int fw_intf_get_exposure( uint32_t ctx_id, uint32_t * val )
{
    return isp_fw_do_get_exposure( ctx_id, val );
}

int fw_intf_get_isp_modules_fr_sharpen_strength( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_FR_SHARPEN_STRENGTH )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_FR_SHARPEN_STRENGTH, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_FR_SHARPEN_STRENGTH  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_ds1_sharpen_strength( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_DS1_SHARPEN_STRENGTH )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_DS1_SHARPEN_STRENGTH, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_DS1_SHARPEN_STRENGTH  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_bypass( uint32_t ctx_id, int *ret_val )
{
    uint64_t *value = (uint64_t *)ret_val;
    int i = 1,ret = 0;
    if (isp_modules_by_pass_get_cmd) {
        acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();
        for (i = 1;i < sizeof(acamera_isp_top_bypass_read)/sizeof(acamera_isp_top_bypass_read[0]);i++) {
            if (isp_modules_by_pass_get_cmd & (1LL << i)) {
                ret=acamera_isp_top_bypass_read[i](context_ptr->settings.isp_base);
                if (ret) {
                    value[0] = isp_modules_by_pass_get_cmd;
                    value[1] |= (1LL<<i);
                    ret = 0;
                }
            }
        }
        isp_modules_by_pass_get_cmd = 0;
    }
    return SUCCESS;
}

int fw_intf_get_customer_sensor_mode( uint32_t ctx_id )
{
    return isp_fw_do_get_sensor_dynamic_mode( ctx_id );
}

int fw_intf_get_customer_antiflicker( uint32_t ctx_id )
{
    return isp_fw_do_get_sensor_antiflicker( ctx_id );
}

int fw_intf_set_customer_temper_mode(uint32_t ctx_id, uint32_t ctrl_val)
{

    settings[ctx_id].temper_frames_number = ctrl_val;

    return isp_fw_do_set_temper_mode(ctx_id, ctrl_val);
}

int fw_intf_get_custom_snr_manual(uint32_t ctx_id)
{
    return isp_fw_do_get_snr_manual(ctx_id);
}

int fw_intf_get_custom_snr_strength(uint32_t ctx_id)
{
    return isp_fw_do_get_snr_strength(ctx_id);
}

int fw_intf_get_custom_tnr_manual(uint32_t ctx_id)
{
    return isp_fw_do_get_tnr_manual(ctx_id);
}

int fw_intf_get_custom_tnr_offset(uint32_t ctx_id)
{
    return isp_fw_do_get_tnr_offset(ctx_id);
}

int fw_intf_get_custom_temper_mode(uint32_t ctx_id)
{
    return settings[ctx_id].temper_frames_number;
}

int fw_intf_set_system_freeze_firmware( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_FREEZE_FIRMWARE )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_FREEZE_FIRMWARE, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_FREEZE_FIRMWARE to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_exposure( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_EXPOSURE )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_EXPOSURE, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_EXPOSURE to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_integration_time( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_INTEGRATION_TIME )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_INTEGRATION_TIME, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_INTEGRATION_TIME to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_max_integration_time( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_MAX_INTEGRATION_TIME )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_MAX_INTEGRATION_TIME, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_MAX_INTEGRATION_TIME to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_sensor_analog_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_SENSOR_ANALOG_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_SENSOR_ANALOG_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_SENSOR_ANALOG_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_sensor_digital_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_isp_digital_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_ISP_DIGITAL_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_ISP_DIGITAL_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_ISP_DIGITAL_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_exposure_ratio( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_EXPOSURE_RATIO )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_EXPOSURE_RATIO, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_EXPOSURE_RATIO to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_awb( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_AWB )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_AWB, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_AWB to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_ccm( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_CCM )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_CCM, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_CCM to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_manual_saturation( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_SATURATION )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_SATURATION, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MANUAL_SATURATION to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_max_exposure_ratio( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_EXPOSURE_RATIO )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_EXPOSURE_RATIO, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MAX_EXPOSURE_RATIO to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_exposure( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_EXPOSURE )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_EXPOSURE, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_EXPOSURE to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_integration_time( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_INTEGRATION_TIME )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_INTEGRATION_TIME, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_INTEGRATION_TIME to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_exposure_ratio( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_EXPOSURE_RATIO )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_EXPOSURE_RATIO, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_EXPOSURE_RATIO to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_max_integration_time( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_INTEGRATION_TIME )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_INTEGRATION_TIME, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MAX_INTEGRATION_TIME to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_sensor_analog_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_SENSOR_ANALOG_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_SENSOR_ANALOG_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_SENSOR_ANALOG_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_max_sensor_analog_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_SENSOR_ANALOG_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_SENSOR_ANALOG_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MAX_SENSOR_ANALOG_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_sensor_digital_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_SENSOR_DIGITAL_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_SENSOR_DIGITAL_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_SENSOR_DIGITAL_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_max_sensor_digital_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_SENSOR_DIGITAL_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_SENSOR_DIGITAL_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MAX_SENSOR_DIGITAL_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_isp_digital_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_ISP_DIGITAL_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_ISP_DIGITAL_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_ISP_DIGITAL_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_max_isp_digital_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_ISP_DIGITAL_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_ISP_DIGITAL_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_MAX_ISP_DIGITAL_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_awb_red_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_AWB_RED_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_AWB_RED_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_AWB_RED_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_awb_green_even_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_AWB_GREEN_EVEN_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_AWB_GREEN_EVEN_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_AWB_GREEN_EVEN_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_awb_green_odd_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_AWB_GREEN_ODD_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_AWB_GREEN_ODD_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_AWB_GREEN_ODD_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_awb_blue_gain( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_AWB_BLUE_GAIN )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_AWB_BLUE_GAIN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_AWB_BLUE_GAIN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_ccm_matrix( uint32_t ctx_id, int32_t *val_ptr, uint32_t elems )
{
    int rc = -1;

    if ( elems != ISP_V4L2_CCM_MATRIX_SZ ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( TSYSTEM )

#if defined( SYSTEM_CCM_MATRIX_RR )
#if defined( SYSTEM_CCM_MATRIX_RG )
#if defined( SYSTEM_CCM_MATRIX_RB )
#if defined( SYSTEM_CCM_MATRIX_GR )
#if defined( SYSTEM_CCM_MATRIX_GG )
#if defined( SYSTEM_CCM_MATRIX_GG )
#if defined( SYSTEM_CCM_MATRIX_BR )
#if defined( SYSTEM_CCM_MATRIX_BG )
#if defined( SYSTEM_CCM_MATRIX_BB )

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_RR, val_ptr[0] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_RR to %d rc: %d",
             val_ptr[0], rc );
        return rc;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_RG, val_ptr[1] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_RG to %d rc: %d",
             val_ptr[1], rc );
        return rc;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_RB, val_ptr[2] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_RB to %d rc: %d",
             val_ptr[2], rc );
        return rc;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_GR, val_ptr[3] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_GR to %d rc: %d",
             val_ptr[3], rc );
        return rc;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_GG, val_ptr[4] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_GG to %d rc: %d",
             val_ptr[4], rc );
        return rc;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_GB, val_ptr[5] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_GB to %d rc: %d",
             val_ptr[5], rc );
        return rc;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_BR, val_ptr[6] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_BR to %d rc: %d",
             val_ptr[6], rc );
        return rc;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_BG, val_ptr[7] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_BG to %d rc: %d",
             val_ptr[7], rc );
        return rc;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_BB, val_ptr[8] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_CCM_MATRIX_BB to %d rc: %d",
             val_ptr[8], rc );
        return rc;
    }

#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#endif // TSYSTEM

    return rc;
}

int fw_intf_set_shading_strength( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSCENE_MODES ) && defined( SHADING_STRENGTH_ID )
    rc = isp_fw_do_set_cmd( ctx_id, TSCENE_MODES, SHADING_STRENGTH_ID, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SHADING_STRENGTH_ID to %d rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_rgb_gamma_lut( uint32_t ctx_id, u16 *val_ptr, uint32_t elems )
{
    uint8_t rc = -1;

    if ( elems != ISP_V4L2_RGB_GAMMA_LUT_SZ ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( CALIBRATION_GAMMA )
    uint32_t ret_value;

    rc = acamera_api_calibration( ctx_id, 0, CALIBRATION_GAMMA, COMMAND_SET, val_ptr, elems * sizeof( *val_ptr ), &ret_value );
    if ( rc || ret_value ) {
        LOG( LOG_ERR, "Failed to set CALIBRATION_GAMMA rc: %d ret_value: %d", rc, ret_value );
        return -1;
    }
#endif

    return rc;
}

// ------------------------------------------------------------------------------ //
//        TANSFER FROM CTRL INTERFACE IQ ARRAY OFFSET
// ------------------------------------------------------------------------------ //

int fw_intf_set_system_calibration_dynamic( uint32_t ctx_id, uint16_t *val_ptr, uint32_t elems )
{
    uint8_t rc = -1;
    uint32_t ret_value,size;
    uint32_t calib_id = val_ptr[IQ_ID_OFFSET];
    void *data = (void *)(val_ptr+IQ_ARRAY_DATA_OFFSET);
    uint32_t element_size = val_ptr[IQ_ELEMENT_SIZE_OFFSET];
    uint32_t array_size = val_ptr[IQ_ARRAY_SIZE_OFFSET];
    if (calib_id == CALIBRATION_SET_INFO_FOR_GET_CALIBRATION) {
        get_cmd_para.calib_id = val_ptr[IQ_ARRAY_SET_GET_CMD];
        get_cmd_para.element_size = element_size;
        get_cmd_para.array_s = array_size;
        return 0;
    }

    memset(&get_cmd_para, 0, sizeof(get_cmd_para));
    acamera_command( ctx_id, TSYSTEM, BUFFER_DATA_TYPE, calib_id, COMMAND_GET, &ret_value );
    size = ((ret_value >> 30) + 1) * ((ret_value >> 15) & 0x7FFF ) * (ret_value & 0x7FFF);

    if (element_size*array_size >= size)
        rc = acamera_api_calibration( ctx_id, 0, calib_id, COMMAND_SET, data, size, &ret_value );
    if ( rc || ret_value ) {
        LOG( LOG_INFO, "Failed to set CALIBRATION ID 0x%x DATA rc: %d ret_value: %d\n",calib_id, rc, ret_value );
        LOG( LOG_INFO, "element_size: %d array_size: %d\n",element_size, array_size );
        return -1;
    }
    return rc;
}

int fw_intf_get_system_calibration_dynamic( uint32_t ctx_id, uint16_t *val_ptr, uint32_t elems )
{
    uint8_t rc = -1;

    uint32_t ret_value,size;

    uint32_t calib_id = get_cmd_para.calib_id;

    uint32_t element_size = get_cmd_para.element_size;
    uint32_t array_size = get_cmd_para.array_s;

    acamera_command( ctx_id, TSYSTEM, BUFFER_DATA_TYPE, calib_id, COMMAND_GET, &ret_value );
    size = ((ret_value >> 30) + 1) * ((ret_value >> 15) & 0x7FFF ) * (ret_value & 0x7FFF);
    if (element_size*array_size >= size)
        rc = acamera_api_calibration( ctx_id, 0, calib_id, COMMAND_GET, val_ptr, size, &ret_value );

    if ( rc || ret_value ) {
        LOG( LOG_INFO, "Failed to get CALIBRATION ID 0x%x DATA rc: %d ret_value: %d",calib_id, rc, ret_value );
        LOG( LOG_INFO, "element_size %d array_size: %d ret:%d\n",element_size,array_size);
        return -1;
    }
    return rc;
}

int fw_intf_set_saturation_target( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_SATURATION_TARGET )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_SATURATION_TARGET, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_SATURATION_TARGET to %d rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_iridix( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_IRIDIX )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_IRIDIX, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_IRIDIX to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_sinter( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_SINTER )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SINTER, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_SINTER to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_temper( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_TEMPER )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_TEMPER, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_TEMPER to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_auto_level( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_AUTO_LEVEL )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_AUTO_LEVEL, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_AUTO_LEVEL to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_frame_stitch( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_FRAME_STITCH )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_FRAME_STITCH, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_FRAME_STITCH to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_raw_frontend( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_RAW_FRONTEND )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_RAW_FRONTEND, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_RAW_FRONTEND to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_black_level( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_BLACK_LEVEL )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_BLACK_LEVEL, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_BLACK_LEVEL to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_shading( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_SHADING )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SHADING, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_SHADING to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_demosaic( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_DEMOSAIC )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_DEMOSAIC, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_DEMOSAIC to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_cnr( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_CNR )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_CNR, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_CNR to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_sharpen( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_SHARPEN )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SHARPEN, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_SHARPEN to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_manual_pf( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_PF )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_PF, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_MANUAL_PF to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_image_resize_enable( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_ENABLE_ID )
    rc = 0;
    uint32_t run = val ? RUN : 0;
    if ( run ) { // no need to set DONE, otherwise, rc will be failed.
        rc = isp_fw_do_set_cmd( ctx_id, TIMAGE, IMAGE_RESIZE_ENABLE_ID, run );
        if ( rc ) {
            LOG( LOG_ERR, "Failed to set IMAGE_RESIZE_ENABLE_ID to %d, rc: %d.", val, rc );
        }
    }
#endif

    return rc;
}

int fw_intf_set_image_resize_type( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_TYPE_ID )
    uint32_t type;
    switch ( val ) {
    case V4L2_RESIZE_TYPE_CROP_FR:
        type = CROP_FR;
        break;
#if ISP_HAS_DS1
    case V4L2_RESIZE_TYPE_CROP_DS1:
        type = CROP_DS;
        break;
    case V4L2_RESIZE_TYPE_SCALER_DS1:
        type = SCALER;
        break;
#endif
    default:
        LOG( LOG_ERR, "Not supported IMAGE_RESIZE_TYPE_ID: %d.", val );
        return -2;
    }

    rc = isp_fw_do_set_cmd( ctx_id, TIMAGE, IMAGE_RESIZE_TYPE_ID, type );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set IMAGE_RESIZE_TYPE_ID to %d, rc: %d.", type, rc );
    }
#endif

    return rc;
}

int fw_intf_set_image_crop_xoffset( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_CROP_XOFFSET_ID )
    rc = isp_fw_do_set_cmd( ctx_id, TIMAGE, IMAGE_CROP_XOFFSET_ID, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set IMAGE_CROP_XOFFSET_ID to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_image_crop_yoffset( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_CROP_YOFFSET_ID )
    rc = isp_fw_do_set_cmd( ctx_id, TIMAGE, IMAGE_CROP_YOFFSET_ID, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set IMAGE_CROP_YOFFSET_ID to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_image_resize_width( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_WIDTH_ID )
    rc = isp_fw_do_set_cmd( ctx_id, TIMAGE, IMAGE_RESIZE_WIDTH_ID, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set IMAGE_RESIZE_WIDTH_ID to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_image_resize_height( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_HEIGHT_ID )
    rc = isp_fw_do_set_cmd( ctx_id, TIMAGE, IMAGE_RESIZE_HEIGHT_ID, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set IMAGE_RESIZE_HEIGHT_ID to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_ae_compensation( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( AE_COMPENSATION_ID )
    rc = isp_fw_do_set_cmd( ctx_id, TALGORITHMS, AE_COMPENSATION_ID, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set AE_COMPENSATION_ID to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_dynamic_gamma_enable( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_DYNAMIC_GAMMA_ENABLE )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_DYNAMIC_GAMMA_ENABLE, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_DYNAMIC_GAMMA_ENABLE to %d, rc: %d.", val, rc );
    }
#endif

    return rc;
}
int fw_intf_set_isp_modules_swhw_registers( uint32_t ctx_id, u16 *val_ptr, uint32_t elems )
{

    uint32_t * array = ( uint32_t * ) val_ptr;
    memset( &swhw_registers_cmd, 0x0, sizeof( swhw_registers_cmd ) );
    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();
    swhw_registers_cmd.isget = array[0];
    if ( swhw_registers_cmd.isget == 1 ) {
        swhw_registers_cmd.addr = array[1];
        return SUCCESS;
    }
    uint32_t addr = array[1];
    uint32_t data = array[2];
    uint32_t addr_align = addr & ~3;
    if ( ( addr < ISP_CONFIG_LUT_OFFSET ) ||
         ( addr > ISP_CONFIG_PING_OFFSET + 2 * ISP_CONFIG_PING_SIZE ) ) {
        system_hw_write_32( addr_align, data );
    } else {
        // read from software context
        uintptr_t sw_addr = context_ptr->settings.isp_base + addr_align;
        system_sw_write_32( sw_addr, data );
    }

    return SUCCESS;
}

int fw_intf_set_sensor_vmax_fps( uint32_t ctx_id, int val )
{
    int rc = -1;
    #if 0
    dma_handle *p_dma = NULL;
    dma_pipe *pipe = NULL;
    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    acamera_fsm_mgr_t *fsm_mgr = &(context_ptr->fsm_mgr);
    fsm_common_t *fsm_arr = fsm_mgr->fsm_arr[FSM_ID_DMA_WRITER];
    dma_writer_fsm_t * dma_writer_fsm = (dma_writer_fsm_t *)(fsm_arr->p_fsm);
    p_dma = dma_writer_fsm->handle;
    #endif

#if defined( TSENSOR ) && defined( SENSOR_VMAX_FPS )
    rc = isp_fw_do_set_cmd( ctx_id, TSENSOR, SENSOR_VMAX_FPS, (uint32_t)val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SENSOR_VMAX_FPS, rc: %d.", rc );
    } else {
    #if 1
        acamera_api_set_fps(ctx_id, dma_fr, (uint32_t)val, 0);/*note: 0- only update c_fps*/
        acamera_api_set_fps(ctx_id, dma_ds1, (uint32_t)val, 0);/*note: 0- only update c_fps*/
    #else
        pipe = &p_dma->pipe[dma_fr];
        pipe->settings.c_fps = val;
        pipe = &p_dma->pipe[dma_ds1];
        pipe->settings.c_fps = val;
    #endif
    }
#endif

    return rc;
}

int fw_intf_get_test_pattern( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( TEST_PATTERN_ENABLE_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, TEST_PATTERN_ENABLE_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get TEST_PATTERN_ENABLE_ID, rc: %d.", rc );
    } else {
        *ret_val = ( *ret_val == ON ) ? 1 : 0;
    }
#endif

    return rc;
}

int fw_intf_get_test_pattern_type( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( TEST_PATTERN_MODE_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, TEST_PATTERN_MODE_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get TEST_PATTERN_MODE_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_sensor_preset( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSENSOR ) && defined( SENSOR_PRESET )
    rc = isp_fw_do_get_cmd( ctx_id, TSENSOR, SENSOR_PRESET, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SENSOR_PRESET, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_af_roi( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( AF_ROI_ID )
    uint32_t roi = 0;
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AF_ROI_ID, &roi );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AF_ROI_ID, rc: %d.", rc );
    } else {
        // map [0, 254] to [0,127] due to limitaton of V4L2_CTRL_TYPE_INTEGER.
        *ret_val = roi >> 1;
    }
#endif

    return rc;
}

int fw_intf_get_output_fr_on_off( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( FR_FORMAT_BASE_PLANE_ID )
    uint32_t format = 0;
    rc = isp_fw_do_get_cmd( ctx_id, TIMAGE, FR_FORMAT_BASE_PLANE_ID, &format );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get FR_FORMAT_BASE_PLANE_ID, rc: %d.", rc );
    } else {
        *ret_val = ( format != DMA_DISABLE ) ? 1 : 0;
    }
#endif

    return rc;
}

int fw_intf_get_output_ds1_on_off( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if ISP_HAS_DS1 && defined( TIMAGE ) && defined( DS1_FORMAT_BASE_PLANE_ID )
    uint32_t format = 0;
    rc = isp_fw_do_get_cmd( ctx_id, TIMAGE, DS1_FORMAT_BASE_PLANE_ID, &format );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get DS1_FORMAT_BASE_PLANE_ID, rc: %d.", rc );
    } else {
        *ret_val = ( format != DMA_DISABLE ) ? 1 : 0;
    }
#endif

    return rc;
}

int fw_intf_get_temper3_mode( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( TEMPER_MODE_ID )
    uint32_t mode = 0;
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, TEMPER_MODE_ID, &mode );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get TEMPER_MODE_ID, rc: %d.", rc );
    } else {
        *ret_val = ( mode == TEMPER3_MODE ) ? 1 : 0;
    }
#endif

    return rc;
}

int fw_intf_get_system_freeze_firmware( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_FREEZE_FIRMWARE )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_FREEZE_FIRMWARE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_FREEZE_FIRMWARE, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_exposure( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_EXPOSURE )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_EXPOSURE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_EXPOSURE, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_integration_time( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_INTEGRATION_TIME )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_INTEGRATION_TIME, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_INTEGRATION_TIME, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_max_integration_time( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_MAX_INTEGRATION_TIME )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_MAX_INTEGRATION_TIME, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_MAX_INTEGRATION_TIME, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_sensor_analog_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_SENSOR_ANALOG_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_SENSOR_ANALOG_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_SENSOR_ANALOG_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_sensor_digital_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_isp_digital_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_ISP_DIGITAL_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_ISP_DIGITAL_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_ISP_DIGITAL_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_exposure_ratio( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_EXPOSURE_RATIO )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_EXPOSURE_RATIO, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_EXPOSURE_RATIO, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_awb( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_AWB )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_AWB, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_AWB, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_ccm( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_CCM )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_CCM, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_CCM, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_manual_saturation( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MANUAL_SATURATION )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MANUAL_SATURATION, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MANUAL_SATURATION, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_max_exposure_ratio( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_EXPOSURE_RATIO )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_EXPOSURE_RATIO, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MAX_EXPOSURE_RATIO, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_max_integration_time( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_INTEGRATION_TIME )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_INTEGRATION_TIME, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MAX_INTEGRATION_TIME, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_max_sensor_analog_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_SENSOR_ANALOG_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_SENSOR_ANALOG_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MAX_SENSOR_ANALOG_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_max_sensor_digital_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_SENSOR_DIGITAL_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_SENSOR_DIGITAL_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MAX_SENSOR_DIGITAL_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_max_isp_digital_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_MAX_ISP_DIGITAL_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_MAX_ISP_DIGITAL_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_MAX_ISP_DIGITAL_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_iridix( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_IRIDIX )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_IRIDIX, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_IRIDIX, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_sinter( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_SINTER )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SINTER, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_SINTER, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_temper( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_TEMPER )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_TEMPER, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_TEMPER, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_auto_level( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_AUTO_LEVEL )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_AUTO_LEVEL, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_AUTO_LEVEL, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_frame_stitch( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_FRAME_STITCH )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_FRAME_STITCH, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_FRAME_STITCH, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_raw_frontend( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_RAW_FRONTEND )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_RAW_FRONTEND, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_RAW_FRONTEND, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_black_level( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_BLACK_LEVEL )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_BLACK_LEVEL, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_BLACK_LEVEL, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_shading( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_SHADING )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SHADING, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_SHADING, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_demosaic( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_DEMOSAIC )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_DEMOSAIC, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_DEMOSAIC, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_cnr( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_CNR )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_CNR, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_CNR, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_sharpen( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_SHARPEN )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SHARPEN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_SHARPEN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_manual_pf( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_MANUAL_PF )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_PF, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_MANUAL_PF  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_status_iso( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSTATUS ) && defined( STATUS_INFO_ISO_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, STATUS_INFO_ISO_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get STATUS_INFO_ISO_ID  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_image_resize_enable( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_ENABLE_ID )
    uint32_t status = 0;
    rc = isp_fw_do_get_cmd( ctx_id, TIMAGE, IMAGE_RESIZE_ENABLE_ID, &status );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get IMAGE_RESIZE_ENABLE_ID, rc: %d.", rc );
    } else {
        *ret_val = ( status == RUN ) ? 1 : 0;
    }
#endif

    return rc;
}

int fw_intf_get_image_resize_type( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_TYPE_ID )
    uint32_t type = 0;
    rc = isp_fw_do_get_cmd( ctx_id, TIMAGE, IMAGE_RESIZE_TYPE_ID, &type );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get IMAGE_RESIZE_TYPE_ID, rc: %d.", rc );
    } else {
        switch ( type ) {
        case CROP_FR:
            *ret_val = V4L2_RESIZE_TYPE_CROP_FR;
            break;
#if ISP_HAS_DS1
        case CROP_DS:
            *ret_val = V4L2_RESIZE_TYPE_CROP_DS1;
            break;
        case SCALER:
            *ret_val = V4L2_RESIZE_TYPE_SCALER_DS1;
            break;
#endif
        default:
            LOG( LOG_ERR, "Not supported IMAGE_RESIZE_TYPE_ID: %d.", type );
            rc = -2;
        }
    }
#endif

    return rc;
}

int fw_intf_get_image_crop_xoffset( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_CROP_XOFFSET_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TIMAGE, IMAGE_CROP_XOFFSET_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get IMAGE_CROP_XOFFSET_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_image_crop_yoffset( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_CROP_YOFFSET_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TIMAGE, IMAGE_CROP_YOFFSET_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get IMAGE_CROP_YOFFSET_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_image_resize_width( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_WIDTH_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TIMAGE, IMAGE_RESIZE_WIDTH_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get IMAGE_RESIZE_WIDTH_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_image_resize_height( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TIMAGE ) && defined( IMAGE_RESIZE_HEIGHT_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TIMAGE, IMAGE_RESIZE_HEIGHT_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get IMAGE_RESIZE_HEIGHT_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_exposure( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_EXPOSURE )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_EXPOSURE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_EXPOSURE, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_integration_time( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_INTEGRATION_TIME )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_INTEGRATION_TIME, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_INTEGRATION_TIME, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_long_integration_time( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_LONG_INTEGRATION_TIME )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_LONG_INTEGRATION_TIME, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_LONG_INTEGRATION_TIME, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_short_integration_time( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_SHORT_INTEGRATION_TIME )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_SHORT_INTEGRATION_TIME, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_SHORT_INTEGRATION_TIME, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_exposure_ratio( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_EXPOSURE_RATIO )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_EXPOSURE_RATIO, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_EXPOSURE_RATIO, rc: %d.", rc );
    }
#endif

    return rc;
}


int fw_intf_get_system_sensor_analog_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_SENSOR_ANALOG_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_SENSOR_ANALOG_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_SENSOR_ANALOG_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}


int fw_intf_get_system_sensor_digital_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_SENSOR_DIGITAL_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_SENSOR_DIGITAL_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_SENSOR_DIGITAL_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}


int fw_intf_get_system_isp_digital_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_ISP_DIGITAL_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_ISP_DIGITAL_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_ISP_DIGITAL_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}


int fw_intf_get_system_awb_red_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_AWB_RED_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_AWB_RED_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_AWB_RED_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_awb_green_even_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_AWB_GREEN_EVEN_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_AWB_GREEN_EVEN_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_AWB_GREEN_EVEN_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_awb_green_odd_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_AWB_GREEN_ODD_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_AWB_GREEN_ODD_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_AWB_GREEN_ODD_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_awb_blue_gain( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_AWB_BLUE_GAIN )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_AWB_BLUE_GAIN, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_AWB_BLUE_GAIN, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_ccm_matrix( uint32_t ctx_id, int32_t *ret_val_ptr, uint32_t elems )
{
    int rc = -1;

    if ( elems != ISP_V4L2_CCM_MATRIX_SZ ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( TSYSTEM )

#if defined( SYSTEM_CCM_MATRIX_RR )
#if defined( SYSTEM_CCM_MATRIX_RG )
#if defined( SYSTEM_CCM_MATRIX_RB )
#if defined( SYSTEM_CCM_MATRIX_GR )
#if defined( SYSTEM_CCM_MATRIX_GG )
#if defined( SYSTEM_CCM_MATRIX_GG )
#if defined( SYSTEM_CCM_MATRIX_BR )
#if defined( SYSTEM_CCM_MATRIX_BG )
#if defined( SYSTEM_CCM_MATRIX_BB )

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_RR, &ret_val_ptr[0] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_RG, &ret_val_ptr[1] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_RB, &ret_val_ptr[2] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_GR, &ret_val_ptr[3] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_GG, &ret_val_ptr[4] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_GB, &ret_val_ptr[5] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_BR, &ret_val_ptr[6] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_BG, &ret_val_ptr[7] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_CCM_MATRIX_BB, &ret_val_ptr[8] );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_CCM_MATRIX_RR, rc: %d", rc );
        return rc;
    }

#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#endif // TSYSTEM

    return rc;
}

int fw_intf_get_shading_strength( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSCENE_MODES ) && defined( SHADING_STRENGTH_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSCENE_MODES, SHADING_STRENGTH_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SHADING_STRENGTH_ID rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_rgb_gamma_lut( uint32_t ctx_id, u16 *ret_val_ptr, uint32_t elems )
{
    int rc = -1;

    if ( elems != ISP_V4L2_RGB_GAMMA_LUT_SZ ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( CALIBRATION_GAMMA )
    uint32_t ret_value;

    rc = acamera_api_calibration( ctx_id, 0, CALIBRATION_GAMMA, COMMAND_GET, ret_val_ptr, elems * sizeof( *ret_val_ptr ), &ret_value );
    if ( rc || ret_value ) {
        LOG( LOG_ERR, "Failed to get CALIBRATION_GAMMA rc: %d ret_value: %d", rc, ret_value );
        return -1;
    }
#endif

    return rc;
}

int fw_intf_get_customer_ae_zone_weight( uint32_t ctx_id, uint32_t *ret_val_ptr, uint32_t elems )
{
    int rtn = -1;

    rtn = fw_intf_get_ae_zone_weight(ctx_id, ret_val_ptr);

    return rtn;
}

int fw_intf_get_customer_awb_zone_weight( uint32_t ctx_id, uint32_t *ret_val_ptr, uint32_t elems )
{
    int rtn = -1;

    rtn = fw_intf_get_awb_zone_weight(ctx_id, ret_val_ptr);

    return rtn;
}


int fw_intf_get_saturation_target( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_SATURATION_TARGET )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_SATURATION_TARGET, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SHADING_STRENGTH_ID rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_status_info_exposure_log2( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSTATUS ) && defined( STATUS_INFO_EXPOSURE_LOG2_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, STATUS_INFO_EXPOSURE_LOG2_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get STATUS_INFO_EXPOSURE_LOG2_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_status_info_gain_log2( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSTATUS ) && defined( STATUS_INFO_GAIN_LOG2_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, STATUS_INFO_GAIN_LOG2_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get STATUS_INFO_GAIN_LOG2_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_status_info_gain_ones( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSTATUS ) && defined( STATUS_INFO_GAIN_ONES_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, STATUS_INFO_GAIN_ONES_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get STATUS_INFO_GAIN_ONES_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_status_info_awb_mix_light_contrast( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSTATUS ) && defined( STATUS_INFO_AWB_MIX_LIGHT_CONTRAST )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, STATUS_INFO_AWB_MIX_LIGHT_CONTRAST, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get STATUS_INFO_AWB_MIX_LIGHT_CONTRAST, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_status_info_af_lens_pos( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSTATUS ) && defined( STATUS_INFO_AF_LENS_POS )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, STATUS_INFO_AF_LENS_POS, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get STATUS_INFO_AF_LENS_POS, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_status_info_af_focus_value( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSTATUS ) && defined( STATUS_INFO_AF_FOCUS_VALUE )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, STATUS_INFO_AF_FOCUS_VALUE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get STATUS_INFO_AF_FOCUS_VALUE, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_info_fw_revision( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSELFTEST ) && defined( FW_REVISION )
    rc = isp_fw_do_get_cmd( ctx_id, TSELFTEST, FW_REVISION, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get FW_REVISION, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_ae_compensation( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( AE_COMPENSATION_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AE_COMPENSATION_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AE_COMPENSATION_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_dynamic_gamma_enable( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_DYNAMIC_GAMMA_ENABLE )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_DYNAMIC_GAMMA_ENABLE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_DYNAMIC_GAMMA_ENABLE, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_af_mode_id( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( AF_MODE_ID )

    uint32_t af_mode_id_val;

    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AF_MODE_ID, &af_mode_id_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AF_MODE_ID, rc: %d.", rc );
        return rc;
    }

    switch (af_mode_id_val) {
    case AF_AUTO_SINGLE:
        *ret_val = ISP_V4L2_AF_MODE_AUTO_SINGLE;
        break;
    case AF_AUTO_CONTINUOUS:
        *ret_val = ISP_V4L2_AF_MODE_AUTO_CONTINUOUS;
        break;
    case AF_MANUAL:
        *ret_val = ISP_V4L2_AF_MODE_MANUAL;
        break;
    default:
        LOG( LOG_ERR, "unknown af_mode_id_val: %d", af_mode_id_val );
        return -1;
    };

#endif

    return rc;
}

int fw_intf_get_awb_mode_id( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( AWB_MODE_ID )
    uint32_t awb_mode_id_val;

    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AWB_MODE_ID, &awb_mode_id_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AWB_MODE_ID, rc: %d.", rc );
        return rc;
    }

    switch ( awb_mode_id_val ) {
    case AWB_AUTO:
        *ret_val = ISP_V4L2_AWB_MODE_AUTO;
        break;
    case AWB_MANUAL:
        *ret_val = ISP_V4L2_AWB_MODE_MANUAL;
        break;
    case AWB_DAY_LIGHT:
        *ret_val = ISP_V4L2_AWB_MODE_DAY_LIGHT;
        break;
    case AWB_CLOUDY:
        *ret_val = ISP_V4L2_AWB_MODE_CLOUDY;
        break;
    case AWB_INCANDESCENT:
        *ret_val = ISP_V4L2_AWB_MODE_INCANDESCENT;
        break;
    case AWB_FLOURESCENT:
        *ret_val = ISP_V4L2_AWB_MODE_FLUORESCENT;
        break;
    case AWB_TWILIGHT:
        *ret_val = ISP_V4L2_AWB_MODE_TWILIGHT;
        break;
    case AWB_SHADE:
        *ret_val = ISP_V4L2_AWB_MODE_SHADE;
        break;
    case AWB_WARM_FLOURESCENT:
        *ret_val = ISP_V4L2_AWB_MODE_WARM_FLUORESCENT;
        break;
    default:
        LOG( LOG_ERR, "unknown awb_mode_id_val: %d", awb_mode_id_val );
        return -1;
    };

#endif

    return rc;
}

int fw_intf_get_awb_temperature( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( AWB_TEMPERATURE_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AWB_TEMPERATURE_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AE_COMPENSATION_ID, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_noise_reduction_mode_id( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( NOISE_REDUCTION_MODE_ID )
    uint32_t nr_mode_id_val;

    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, NOISE_REDUCTION_MODE_ID, &nr_mode_id_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get NOISE_REDUCTION_MODE_ID, rc: %d.", rc );
        return rc;
    }

    switch ( nr_mode_id_val ) {
    case NOISE_REDUCTION_OFF:
        *ret_val = ISP_V4L2_NOISE_REDUCTION_MODE_OFF;
        break;
    case NOISE_REDUCTION_ON:
        *ret_val = ISP_V4L2_NOISE_REDUCTION_MODE_ON;
        break;
    default:
        LOG( LOG_ERR, "unknown nr_mode_id_val: %d", nr_mode_id_val );
        return -1;
    };

#endif

    return rc;
}

int fw_intf_get_af_manual_control( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( AF_MANUAL_CONTROL_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AF_MANUAL_CONTROL_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AF_MANUAL_CONTROL_ID, rc: %d.", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_af_state_id( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;
    uint32_t af_state_id_val;

#if defined( TALGORITHMS ) && defined( AF_STATE_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AF_STATE_ID, &af_state_id_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AF_STATE_ID, rc: %d.", rc );
        return rc;
    }

    switch (af_state_id_val) {
    case AF_INACTIVE:
        *ret_val = ISP_V4L2_AF_STATE_INACTIVE;
        break;
    case AF_SCAN:
        *ret_val = ISP_V4L2_AF_STATE_SCAN;
        break;
    case AF_FOCUSED:
        *ret_val = ISP_V4L2_AF_STATE_FOCUSED;
        break;
    case AF_UNFOCUSED:
        *ret_val = ISP_V4L2_AF_STATE_UNFOCUSED;
        break;
    default:
        LOG( LOG_ERR, "unknown af_state_id_val: %d", af_state_id_val );
        return -1;
    };

#endif

    return rc;
}

int fw_intf_get_ae_state_id( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;
    uint32_t ae_state_id_val;

#if defined( TALGORITHMS ) && defined( AE_STATE_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AE_STATE_ID, &ae_state_id_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AE_STATE_ID, rc: %d.", rc );
        return rc;
    }

    switch (ae_state_id_val) {
    case AE_INACTIVE:
        *ret_val = ISP_V4L2_AE_STATE_INACTIVE;
        break;
    case AE_SEARCHING:
        *ret_val = ISP_V4L2_AE_STATE_SEARCHING;
        break;
    case AE_CONVERGED:
        *ret_val = ISP_V4L2_AE_STATE_CONVERGED;
        break;
    default:
        LOG( LOG_ERR, "unknown ae_state_id_val: %d", ae_state_id_val );
        return -1;
    };

#endif

    return rc;
}

int fw_intf_get_awb_state_id( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;
    uint32_t awb_state_id_val;

#if defined( TALGORITHMS ) && defined( AWB_STATE_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, AWB_STATE_ID, &awb_state_id_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AWB_STATE_ID, rc: %d.", rc );
        return rc;
    }

    switch (awb_state_id_val) {
    case AWB_INACTIVE:
        *ret_val = ISP_V4L2_AWB_STATE_INACTIVE;
        break;
    case AWB_SEARCHING:
        *ret_val = ISP_V4L2_AWB_STATE_SEARCHING;
        break;
    case AWB_CONVERGED:
        *ret_val = ISP_V4L2_AWB_STATE_CONVERGED;
        break;
    default:
        LOG( LOG_ERR, "unknown awb_state_id_val: %d", awb_state_id_val );
        return -1;
    };

#endif

    return rc;
}

int fw_intf_get_lines_per_second( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSENSOR ) && defined( SENSOR_LINES_PER_SECOND )
    rc = isp_fw_do_get_cmd( ctx_id, TSENSOR, SENSOR_LINES_PER_SECOND, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SENSOR_LINES_PER_SECOND rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_sensor_info_physical_width( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSENSOR ) && defined( SENSOR_INFO_PHYSICAL_WIDTH )
    rc = isp_fw_do_get_cmd( ctx_id, TSENSOR, SENSOR_INFO_PHYSICAL_WIDTH, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_sensor_info_physical_height( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSENSOR ) && defined( SENSOR_INFO_PHYSICAL_HEIGHT )
    rc = isp_fw_do_get_cmd( ctx_id, TSENSOR, SENSOR_INFO_PHYSICAL_HEIGHT, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_lens_info_minfocus_distance( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TLENS ) && defined( LENS_INFO_MINFOCUS_DISTANCE )
    rc = isp_fw_do_get_cmd( ctx_id, TLENS, LENS_INFO_MINFOCUS_DISTANCE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_lens_info_hyperfocal_distance( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TLENS ) && defined( LENS_INFO_HYPERFOCAL_DISTANCE )
    rc = isp_fw_do_get_cmd( ctx_id, TLENS, LENS_INFO_HYPERFOCAL_DISTANCE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_lens_info_focal_length( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TLENS ) && defined( LENS_INFO_FOCAL_LENGTH )
    rc = isp_fw_do_get_cmd( ctx_id, TLENS, LENS_INFO_FOCAL_LENGTH, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_lens_info_aperture( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TLENS ) && defined( LENS_INFO_APERTURE )
    rc = isp_fw_do_get_cmd( ctx_id, TLENS, LENS_INFO_APERTURE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_integration_time_limits( uint32_t ctx_id, int *min_val, int *max_val )
{
    int rc = -1;

#if defined( TSENSOR ) && defined( SENSOR_INTEGRATION_TIME_MIN ) && defined( SENSOR_INTEGRATION_TIME_LIMIT )
    rc = isp_fw_do_get_cmd( ctx_id, TSENSOR, SENSOR_INTEGRATION_TIME_MIN, min_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SENSOR_INTEGRATION_TIME_MIN rc: %d", rc );
        return rc;
    }

    rc = isp_fw_do_get_cmd( ctx_id, TSENSOR, SENSOR_INTEGRATION_TIME_LIMIT, max_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SENSOR_INTEGRATION_TIME_LIMIT rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_ae_stats( uint32_t ctx_id, uint32_t *ret_val_ptr, uint32_t elems )
{
    int rc = -1;

    if ( elems != ISP_V4L2_AE_STATS_SIZE ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( AE_STATS_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, AE_STATS_ID, ret_val_ptr );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AE_STATS_ID rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_sensor_vmax_fps( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSENSOR ) && defined( SENSOR_VMAX_FPS )
    rc = isp_fw_do_get_cmd( ctx_id, TSENSOR, SENSOR_VMAX_FPS, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SENSOR_VMAX_FPS, rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_awb_stats( uint32_t ctx_id, uint32_t *ret_val_ptr, uint32_t elems )
{
    int rc = -1;

    if ( elems != ISP_V4L2_AWB_STATS_SIZE ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( AWB_STATS_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, AWB_STATS_ID, ret_val_ptr );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AWB_STATS_ID rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_af_stats( uint32_t ctx_id, uint32_t *ret_val_ptr, uint32_t elems )
{
    int rc = -1;

    if ( elems != ISP_V4L2_AF_STATS_SIZE ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( AWB_STATS_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TSTATUS, AF_STATS_ID, ret_val_ptr );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get AF_STATS_ID rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_md_stats( uint32_t ctx_id, uint32_t *ret_val_ptr, uint32_t elems )
{
    int rc = -1;

    if ( elems != ISP_V4L2_MD_STATS_SIZE ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( MD_STATS_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, MD_STATS_ID, ret_val_ptr );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get MD_STATS_ID rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_flicker_stats( uint32_t ctx_id, uint32_t *ret_val_ptr, uint32_t elems )
{
    int rc = -1;

    if ( elems != ISP_V4L2_FLICKER_STATS_SIZE ) {
        LOG( LOG_ERR, "Bad no. of elems: %d", elems );
        return -1;
    }

#if defined( FLICKER_STATS_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, FLICKER_STATS_ID, ret_val_ptr );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get FLICKER_STATS_ID rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_set_dpc_threshold_slope( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_DPC_THRES_SLOPE )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_DPC_THRES_SLOPE, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_dpc_threshold_slope( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_DPC_THRES_SLOPE )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_DPC_THRES_SLOPE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_set_black_level_r( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_BLACK_LEVEL_R )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_BLACK_LEVEL_R, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_black_level_r( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_BLACK_LEVEL_R )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_BLACK_LEVEL_R, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_set_black_level_gr( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_BLACK_LEVEL_GR )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_BLACK_LEVEL_GR, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_black_level_gr( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_BLACK_LEVEL_GR )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_BLACK_LEVEL_GR, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_set_black_level_gb( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_BLACK_LEVEL_GB )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_BLACK_LEVEL_GB, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_black_level_gb( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_BLACK_LEVEL_GB )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_BLACK_LEVEL_GB, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_set_black_level_b( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_BLACK_LEVEL_B )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_BLACK_LEVEL_B, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_black_level_b( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_BLACK_LEVEL_B )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_BLACK_LEVEL_B, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_set_demosaic_sharp( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_DEMOSAIC_SHARP )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_DEMOSAIC_SHARP, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_demosaic_sharp( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_DEMOSAIC_SHARP )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_DEMOSAIC_SHARP, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_set_cnr_strength( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_CNR_STRENGTH )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_CNR_STRENGTH, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_cnr_strength( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_CNR_STRENGTH )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_CNR_STRENGTH, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_get_system_antiflicker_enable(uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_ANTIFLICKER_ENABLE )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_ANTIFLICKER_ENABLE,ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_ANTIFLICKER_ENABLE  rc: %d.",  rc );
    }
#endif

    return rc;
}

int fw_intf_get_system_antiflicker_frequency( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_ANTI_FLICKER_FREQUENCY )
    rc = isp_fw_do_get_cmd( ctx_id, TSYSTEM, SYSTEM_ANTI_FLICKER_FREQUENCY, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get SYSTEM_ANTI_FLICKER_FREQUENCY  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_iridix_enable( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_IRIDIX_ENABLE )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_IRIDIX_ENABLE, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_IRIDIX_ENABLE  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_get_isp_modules_iridix_strength( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_IRIDIX_STRENGTH )
    rc = isp_fw_do_get_cmd( ctx_id, TISP_MODULES, ISP_MODULES_IRIDIX_STRENGTH, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get ISP_MODULES_IRIDIX_STRENGTH_INROI  rc: %d.", rc );
    }
#endif

    return rc;
}
int fw_intf_get_isp_modules_swhw_registers( uint32_t ctx_id, uint16_t *ret_val )
{
    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    if ( swhw_registers_cmd.isget != 1 ) {
        return SUCCESS;
    }
    uint32_t * array = (uint32_t *) ret_val;
    uint32_t addr = swhw_registers_cmd.addr;
    uint32_t addr_align = addr & ~3;

    if ( ( addr < ISP_CONFIG_LUT_OFFSET ) ||
         ( addr > ISP_CONFIG_PING_OFFSET + 2 * ISP_CONFIG_PING_SIZE ) ) {
        array[2] = system_hw_read_32( addr_align );
    } else {
        // read from software context
        uintptr_t sw_addr = context_ptr->settings.isp_base + addr_align;
        array[2] = system_sw_read_32( sw_addr );
    }
    array[1] = addr_align;
    memset( &swhw_registers_cmd, 0x0, sizeof( swhw_registers_cmd ) );
    return SUCCESS;
}

int fw_intf_set_system_antiflicker_enable(uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_ANTIFLICKER_ENABLE )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_ANTIFLICKER_ENABLE, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_ANTIFLICKER_ENABLE  rc: %d.",  rc );
    }
#endif

    return rc;
}

int fw_intf_set_system_antiflicker_frequency( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TSYSTEM ) && defined( SYSTEM_ANTI_FLICKER_FREQUENCY )
    rc = isp_fw_do_set_cmd( ctx_id, TSYSTEM, SYSTEM_ANTI_FLICKER_FREQUENCY, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SYSTEM_ANTI_FLICKER_FREQUENCY  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_iridix_enable( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_IRIDIX_ENABLE )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_IRIDIX_ENABLE, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_IRIDIX_ENABLE  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_iridix_strength( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_IRIDIX_STRENGTH )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_IRIDIX_STRENGTH, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_IRIDIX_STRENGTH_INROI  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_fr_sharpen_strength( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_FR_SHARPEN_STRENGTH )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_FR_SHARPEN_STRENGTH, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_FR_SHARPEN_STRENGTH  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_ds1_sharpen_strength( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TISP_MODULES ) && defined( ISP_MODULES_DS1_SHARPEN_STRENGTH )
    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_MODULES_DS1_SHARPEN_STRENGTH, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set ISP_MODULES_DS1_SHARPEN_STRENGTH  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_set_isp_modules_bypass( uint32_t ctx_id, uint32_t * val_ptr )
{
    uint64_t i = 0;
    uint64_t *value = (uint64_t *)val_ptr;
    uint64_t cmd  = value[0];
    uint64_t data = value[1];

    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_ctx_ptr(ctx_id);
    if (cmd & ACAMERA_ISP_TOP_ISP_GET_BY_PASS_CMD) {
        isp_modules_by_pass_get_cmd = data;
        return SUCCESS;
    }

    for (i = 1;i < sizeof(acamera_isp_top_bypass_write)/sizeof(acamera_isp_top_bypass_write[0]) ;i++) {
        if (cmd & (1LL<<i)) {
            LOG( LOG_INFO, "isp module by pass cmd 0x%lx i:%d data:0x%lx",cmd,i,data);
            acamera_isp_top_bypass_write[i](context_ptr->settings.isp_base,(data & (1LL << i ))?1:0);
        }
    }
    return SUCCESS;
}

int fw_intf_get_day_night_detect( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( DAYNIGHT_DETECT_ID )
    rc = isp_fw_do_get_cmd( ctx_id, TALGORITHMS, DAYNIGHT_DETECT_ID, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }
#endif

    return rc;
}

int fw_intf_set_day_night_detect( uint32_t ctx_id, int val )
{
    int rc = -1;

#if defined( TALGORITHMS ) && defined( DAYNIGHT_DETECT_ID )
    rc = isp_fw_do_set_cmd( ctx_id, TALGORITHMS, DAYNIGHT_DETECT_ID, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set DAYNIGHT_DETECT_ID  rc: %d.", rc );
    }
#endif

    return rc;
}

int fw_intf_set_scale_crop_startx( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TAML_SCALER, SCALER_STARTX, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SCALER_STARTX  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_scale_crop_starty( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TAML_SCALER, SCALER_STARTY, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SCALER_STARTY  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_scale_crop_width( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TAML_SCALER, SCALER_CROP_WIDTH, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SCALER_CROP_WIDTH  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_scale_crop_height( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TAML_SCALER, SCALER_CROP_HEIGHT, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SCALER_CROP_HEIGHT  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_scale_crop_enable( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TAML_SCALER, SCALER_CROP_ENABLE, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SCALER_CROP_HEIGHT  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_sensor_power( uint32_t ctx_id, int val )
{
    int rc = -1;
    if (val == 1 ) {
        rc = isp_fw_do_set_cmd( ctx_id, TSENSOR, SENSOR_POWER_ON, val );
        if ( rc ) {
            LOG( LOG_ERR, "Failed to set sensor power ON rc: %d.", rc );
        }
    } else {
        rc = isp_fw_do_set_cmd( ctx_id, TSENSOR, SENSOR_POWER_OFF, val );
        if ( rc ) {
            LOG( LOG_ERR, "Failed to set sensor power OFF rc: %d.", rc );
        }
    }
    return rc;
}

int fw_intf_set_sensor_md_en( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TALGORITHMS, SENSOR_MD_EN, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SENSOR_MD_ON  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_sensor_decmpr_en( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TALGORITHMS, SENSOR_DECMPR_EN, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SENSOR_DECMPR_ON  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_sensor_decmpr_lossless_en( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TALGORITHMS, SENSOR_DECMPR_LOSSLESS_EN, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SENSOR_DECMPR_LOSSLESS_EN  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_sensor_flicker_en( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TALGORITHMS, SENSOR_FLICKER_EN, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SENSOR_FLICKER_EN  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_scaler_fps( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TAML_SCALER, SCALER_FPS, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set SC FPS  rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_isp_modules_ae_roi_exact_coordinates( uint32_t ctx_id, uint32_t * val_ptr )
{

    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    acamera_fsm_mgr_t *fsm_mgr = &(context_ptr->fsm_mgr);
    fsm_common_t *fsm_arr = fsm_mgr->fsm_arr[FSM_ID_AE];
    AE_fsm_t * ae_fsm = (AE_fsm_t *)(fsm_arr->p_fsm);
    memcpy((void *) &ae_fsm->aeNnRoiTo3a, (void *)val_ptr, sizeof( struct CropTypeTo3A ));

    return SUCCESS;
}

int fw_intf_set_isp_modules_af_roi_exact_coordinates( uint32_t ctx_id, uint32_t * val_ptr )
{

    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    acamera_fsm_mgr_t *fsm_mgr = &(context_ptr->fsm_mgr);
    fsm_common_t *fsm_arr = fsm_mgr->fsm_arr[FSM_ID_AF];
    AF_fsm_t * af_fsm = (AF_fsm_t *)(fsm_arr->p_fsm);
    memcpy((void *) &af_fsm->afNnRoiTo3a, (void *)val_ptr, sizeof( struct CropTypeTo3A ));

    return SUCCESS;

}

int fw_intf_set_isp_modules_is_capturing( uint32_t ctx_id, int val )
{
    int rc = -1;

    rc = isp_fw_do_set_cmd( ctx_id, TISP_MODULES, ISP_IS_CAPTURING, val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to set is capturing rc: %d.", rc );
    }

    return rc;
}

int fw_intf_set_isp_modules_fps_range( uint32_t ctx_id, uint32_t * val_ptr )
{
    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    acamera_fsm_mgr_t *fsm_mgr = &(context_ptr->fsm_mgr);
    fsm_common_t *fsm_arr = fsm_mgr->fsm_arr[FSM_ID_AE];
    AE_fsm_t * ae_fsm = (AE_fsm_t *)(fsm_arr->p_fsm);
    ae_fsm->fps_range[0] = val_ptr[0];//min fps
    ae_fsm->fps_range[1] = val_ptr[1];//max fps

    return SUCCESS;

}


int fw_intf_get_hist_mean( uint32_t ctx_id, int *ret_val )
{

    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    acamera_fsm_mgr_t *fsm_mgr = &(context_ptr->fsm_mgr);
    fsm_common_t *fsm_arr = fsm_mgr->fsm_arr[FSM_ID_AE];
    AE_fsm_t * ae_fsm = (AE_fsm_t *)(fsm_arr->p_fsm);
    *ret_val = ae_fsm->ae_hist_mean;
    return SUCCESS;
}

int fw_intf_get_ae_target( uint32_t ctx_id, int *ret_val )
{

    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    acamera_fsm_mgr_t *fsm_mgr = &(context_ptr->fsm_mgr);
    fsm_common_t *fsm_arr = fsm_mgr->fsm_arr[FSM_ID_AE];
    AE_fsm_t * ae_fsm = (AE_fsm_t *)(fsm_arr->p_fsm);
    *ret_val = ae_fsm->max_target;
    return SUCCESS;
}

int fw_intf_get_custom_fr_fps( uint32_t ctx_id, int *ret_val )
{

    dma_handle *p_dma = NULL;
    dma_pipe *pipe = NULL;
    acamera_context_ptr_t context_ptr = (acamera_context_ptr_t)acamera_get_api_ctx_ptr();

    acamera_fsm_mgr_t *fsm_mgr = &(context_ptr->fsm_mgr);
    fsm_common_t *fsm_arr = fsm_mgr->fsm_arr[FSM_ID_DMA_WRITER];
    dma_writer_fsm_t * dma_writer_fsm = (dma_writer_fsm_t *)(fsm_arr->p_fsm);
    p_dma = dma_writer_fsm->handle;
    pipe = &p_dma->pipe[dma_fr];
    *ret_val = pipe->settings.t_fps;

    return 0;
}

int fw_intf_get_scale_crop_startx( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

    rc = isp_fw_do_get_cmd( ctx_id, TAML_SCALER, SCALER_STARTX, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }

    return 0;
}

int fw_intf_get_scale_crop_starty( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

    rc = isp_fw_do_get_cmd( ctx_id, TAML_SCALER, SCALER_STARTY, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }

    return 0;
}

int fw_intf_get_scale_crop_width( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

    rc = isp_fw_do_get_cmd( ctx_id, TAML_SCALER, SCALER_CROP_WIDTH, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }

    return 0;
}

int fw_intf_get_scale_crop_height( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

    rc = isp_fw_do_get_cmd( ctx_id, TAML_SCALER, SCALER_CROP_HEIGHT, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }

    return 0;
}

int fw_intf_get_stream_status( uint32_t ctx_id, int *ret_val )
{

    *ret_val = isp_v4l2_get_stream_status( ctx_id );

    return 0;
}

int fw_intf_get_scaler_fps( uint32_t ctx_id, int *ret_val )
{
    int rc = -1;

    rc = isp_fw_do_get_cmd( ctx_id, TAML_SCALER, SCALER_FPS, ret_val );
    if ( rc ) {
        LOG( LOG_ERR, "Failed to get cmd rc: %d", rc );
        return rc;
    }

    return 0;
}

#define MOVE_I_TO_NEXT for (;buf[i] != '\0';i++)
extern uint32_t acamera_get_api_context( void );

static ssize_t isp_proc_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
    ssize_t ret = 0;
    uint32_t i = 0;
    uint32_t ret_val = 0;
    uint32_t ret_ccm[9] = {0};
    uint32_t ctx_id = acamera_get_api_context();
    ret += sprintf(buf,"--------------TSYSTEM----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_LOGGER_LEVEL, 0, COMMAND_GET, &ret_val );
    if ( ret_val == DEBUG ) ret += sprintf(&buf[i],"SYSTEM_LOGGER_LEVEL : DEBUG\n");
    else if ( ret_val == INFO ) ret += sprintf(&buf[i],"SYSTEM_LOGGER_LEVEL : INFO\n");
    else if ( ret_val == NOTICE ) ret += sprintf(&buf[i],"SYSTEM_LOGGER_LEVEL : NOTICE\n");
    else if ( ret_val == WARNING ) ret += sprintf(&buf[i],"SYSTEM_LOGGER_LEVEL : WARNING\n");
    else if ( ret_val == ERROR ) ret += sprintf(&buf[i],"SYSTEM_LOGGER_LEVEL : ERROR\n");
    else if ( ret_val == CRITICAL ) ret += sprintf(&buf[i],"SYSTEM_LOGGER_LEVEL : CRITICAL\n");
    else if ( ret_val == NOTHING ) ret += sprintf(&buf[i],"SYSTEM_LOGGER_LEVEL : NOTHING\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_LOGGER_MASK, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_LOGGER_MASK : %ud\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, BUFFER_DATA_TYPE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"BUFFER_DATA_TYPE : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, TEST_PATTERN_ENABLE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == ON ) ret += sprintf(&buf[i],"TEST_PATTERN_ENABLE_ID : ON\n");
    else if ( ret_val == OFF ) ret += sprintf(&buf[i],"TEST_PATTERN_ENABLE_ID : OFF\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, TEST_PATTERN_MODE_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"TEST_PATTERN_MODE_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, TEMPER_MODE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == TEMPER3_MODE ) ret += sprintf(&buf[i],"TEMPER_MODE_ID : TEMPER3_MODE\n");
    else if ( ret_val == TEMPER2_MODE ) ret += sprintf(&buf[i],"TEMPER_MODE_ID : TEMPER2_MODE\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_FREEZE_FIRMWARE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_FREEZE_FIRMWARE : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_EXPOSURE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_EXPOSURE : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_EXPOSURE_RATIO, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_EXPOSURE_RATIO : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_INTEGRATION_TIME, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_INTEGRATION_TIME : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_MAX_INTEGRATION_TIME, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_MAX_INTEGRATION_TIME : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_SENSOR_ANALOG_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_SENSOR_ANALOG_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_ISP_DIGITAL_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_ISP_DIGITAL_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_ISP_DIGITAL_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_ISP_DIGITAL_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_AWB, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_AWB : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_CCM, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_CCM : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MANUAL_SATURATION, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MANUAL_SATURATION : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_EXPOSURE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_EXPOSURE : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_EXPOSURE_RATIO, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_EXPOSURE_RATIO : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MAX_EXPOSURE_RATIO, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MAX_EXPOSURE_RATIO : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_INTEGRATION_TIME, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_INTEGRATION_TIME : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_LONG_INTEGRATION_TIME, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_LONG_INTEGRATION_TIME : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_SHORT_INTEGRATION_TIME, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_SHORT_INTEGRATION_TIME : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MAX_INTEGRATION_TIME, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MAX_INTEGRATION_TIME : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_SENSOR_ANALOG_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_SENSOR_ANALOG_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MAX_SENSOR_ANALOG_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MAX_SENSOR_ANALOG_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_SENSOR_DIGITAL_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_SENSOR_DIGITAL_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MAX_SENSOR_DIGITAL_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MAX_SENSOR_DIGITAL_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_ISP_DIGITAL_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_ISP_DIGITAL_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_MAX_ISP_DIGITAL_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_MAX_ISP_DIGITAL_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_AWB_RED_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_AWB_RED_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_AWB_GREEN_EVEN_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_AWB_GREEN_EVEN_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_AWB_GREEN_ODD_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_AWB_GREEN_ODD_GAIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_AWB_BLUE_GAIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_AWB_BLUE_GAIN : %d\n",ret_val);
     MOVE_I_TO_NEXT;
    fw_intf_get_system_ccm_matrix( ctx_id, ret_ccm, ISP_V4L2_CCM_MATRIX_SZ);
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_RR : %d\n",ret_ccm[0]);
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_RG : %d\n",ret_ccm[1]);
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_RB : %d\n",ret_ccm[2]);
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_GR : %d\n",ret_ccm[3]);
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_GG : %d\n",ret_ccm[4]);
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_GB : %d\n",ret_ccm[5]);
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_BR : %d\n",ret_ccm[6]);
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_BG : %d\n",ret_ccm[7]);
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"SYSTEM_CCM_MATRIX_BB : %d\n",ret_ccm[8]);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_SATURATION_TARGET, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_SATURATION_TARGET : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_ANTIFLICKER_ENABLE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_ANTIFLICKER_ENABLE : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_ANTI_FLICKER_FREQUENCY, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_ANTI_FLICKER_FREQUENCY : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSYSTEM, SYSTEM_DYNAMIC_GAMMA_ENABLE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SYSTEM_DYNAMIC_GAMMA_ENABLE : %d\n",ret_val);

    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TSENSOR----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_STREAMING, 0, COMMAND_GET, &ret_val );
    if ( ret_val == ON ) ret += sprintf(&buf[i],"SENSOR_STREAMING : ON\n");
    else if ( ret_val == OFF ) ret += sprintf(&buf[i],"SENSOR_STREAMING : OFF\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_SUPPORTED_PRESETS, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_SUPPORTED_PRESETS : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_PRESET, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_PRESET : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_WDR_MODE, 0, COMMAND_GET, &ret_val );
    if ( ret_val == WDR_MODE_LINEAR ) ret += sprintf(&buf[i],"SENSOR_WDR_MODE : WDR_MODE_LINEAR\n");
    else if ( ret_val == WDR_MODE_NATIVE ) ret += sprintf(&buf[i],"SENSOR_WDR_MODE : WDR_MODE_NATIVE\n");
    else if ( ret_val == WDR_MODE_FS_LIN ) ret += sprintf(&buf[i],"SENSOR_WDR_MODE : WDR_MODE_FS_LIN\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_WIDTH, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_WIDTH : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_HEIGHT, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_HEIGHT : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_EXPOSURES, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_EXPOSURES : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INFO_PRESET, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INFO_PRESET : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INFO_WDR_MODE, 0, COMMAND_GET, &ret_val );
    if ( ret_val == 0 ) ret += sprintf(&buf[i],"SENSOR_INFO_WDR_MODE : WDR_MODE_LINEAR\n");
    else if ( ret_val == 1 ) ret += sprintf(&buf[i],"SENSOR_INFO_WDR_MODE : WDR_MODE_NATIVE\n");
    else if ( ret_val == 2 ) ret += sprintf(&buf[i],"SENSOR_INFO_WDR_MODE : WDR_MODE_FS_LIN\n");

    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INFO_FPS, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INFO_FPS : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INFO_WIDTH, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INFO_WIDTH : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INFO_HEIGHT, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INFO_HEIGHT : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INFO_EXPOSURES, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INFO_EXPOSURES : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_LINES_PER_SECOND, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_LINES_PER_SECOND : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INTEGRATION_TIME_MIN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INTEGRATION_TIME_MIN : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INTEGRATION_TIME_LIMIT, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INTEGRATION_TIME_LIMIT : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INFO_PHYSICAL_WIDTH, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INFO_PHYSICAL_WIDTH : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_INFO_PHYSICAL_HEIGHT, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_INFO_PHYSICAL_HEIGHT : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_WDRMODE_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_WDRMODE_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSENSOR, SENSOR_VMAX_FPS, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SENSOR_VMAX_FPS : %d\n",ret_val);

    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TGENERAL----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TGENERAL, CONTEXT_NUMBER, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"CONTEXT_NUMBER : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TGENERAL, ACTIVE_CONTEXT, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ACTIVE_CONTEXT : %d\n",ret_val);

    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TIMAGE----------------\n");

    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, DMA_READER_OUTPUT_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == 0x10 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : DMA_READER_OUT_FR\n");
    else if ( ret_val == 0x11 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : DMA_READER_OUT_DS\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, FR_FORMAT_BASE_PLANE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == DMA_DISABLE ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : DMA_DISABLE\n");
    else if ( ret_val == RGB32 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : RGB32\n");
    else if ( ret_val == A2R10G10B10 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : A2R10G10B10\n");
    else if ( ret_val == RGB565 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : RGB565\n");
    else if ( ret_val == RGB24 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : RGB24\n");
    else if ( ret_val == GEN32 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : GEN32\n");
    else if ( ret_val == RAW16 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : RAW16\n");
    else if ( ret_val == RAW12 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : RAW12\n");
    else if ( ret_val == AYUV ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : AYUV\n");
    else if ( ret_val == Y410 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : Y410\n");
    else if ( ret_val == YUY2 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : YUY2\n");
    else if ( ret_val == UYVY ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : UYVY\n");
    else if ( ret_val == Y210 ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : Y210\n");
    else if ( ret_val == NV12_YUV ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : NV12_YUV\n");
    else if ( ret_val == NV12_YVU ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : NV12_YVU\n");
    else if ( ret_val == YV12_YU ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : YV12_YU\n");
    else if ( ret_val == YV12_YV ) ret += sprintf(&buf[i],"DMA_READER_OUTPUT_ID : YV12_YV\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, DS1_FORMAT_BASE_PLANE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == DMA_DISABLE ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : DMA_DISABLE\n");
    else if ( ret_val == RGB32 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : RGB32\n");
    else if ( ret_val == A2R10G10B10 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : A2R10G10B10\n");
    else if ( ret_val == RGB565 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : RGB565\n");
    else if ( ret_val == RGB24 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : RGB24\n");
    else if ( ret_val == GEN32 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : GEN32\n");
    else if ( ret_val == RAW16 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : RAW16\n");
    else if ( ret_val == RAW12 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : RAW12\n");
    else if ( ret_val == AYUV ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : AYUV\n");
    else if ( ret_val == Y410 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : Y410\n");
    else if ( ret_val == YUY2 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : YUY2\n");
    else if ( ret_val == UYVY ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : UYVY\n");
    else if ( ret_val == Y210 ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : Y210\n");
    else if ( ret_val == NV12_YUV ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : NV12_YUV\n");
    else if ( ret_val == NV12_YVU ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : NV12_YVU\n");
    else if ( ret_val == YV12_YU ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : YV12_YU\n");
    else if ( ret_val == YV12_YV ) ret += sprintf(&buf[i],"DS1_FORMAT_BASE_PLANE_ID : YV12_YV\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, ORIENTATION_VFLIP_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == ENABLE ) ret += sprintf(&buf[i],"ORIENTATION_VFLIP_ID : ENABLE\n");
    else if ( ret_val == DISABLE ) ret += sprintf(&buf[i],"ORIENTATION_VFLIP_ID : DISABLE\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, ORIENTATION_HFLIP_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == ENABLE ) ret += sprintf(&buf[i],"ORIENTATION_HFLIP_ID : ENABLE\n");
    else if ( ret_val == DISABLE ) ret += sprintf(&buf[i],"ORIENTATION_HFLIP_ID : DISABLE\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, IMAGE_RESIZE_TYPE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == CROP_FR ) ret += sprintf(&buf[i],"IMAGE_RESIZE_TYPE_ID : CROP_FR\n");
    else if ( ret_val == CROP_DS ) ret += sprintf(&buf[i],"IMAGE_RESIZE_TYPE_ID : CROP_DS\n");
    else if ( ret_val == SCALER ) ret += sprintf(&buf[i],"IMAGE_RESIZE_TYPE_ID : SCALER\n");
    ret += sprintf(&buf[i],"IMAGE_RESIZE_TYPE_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, IMAGE_RESIZE_ENABLE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == DONE ) ret += sprintf(&buf[i],"IMAGE_RESIZE_ENABLE_ID : DONE\n");
    else if ( ret_val == RUN ) ret += sprintf(&buf[i],"IMAGE_RESIZE_ENABLE_ID : RUN\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, IMAGE_RESIZE_WIDTH_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"IMAGE_RESIZE_WIDTH_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, IMAGE_RESIZE_HEIGHT_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"IMAGE_RESIZE_HEIGHT_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, IMAGE_CROP_XOFFSET_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"IMAGE_CROP_XOFFSET_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TIMAGE, IMAGE_CROP_YOFFSET_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"IMAGE_CROP_YOFFSET_ID : %d\n",ret_val);

    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TALGORITHMS----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AE_MODE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == AE_AUTO ) ret += sprintf(&buf[i],"AE_MODE_ID : AE_AUTO\n");
    else if ( ret_val == AE_FULL_MANUAL ) ret += sprintf(&buf[i],"AE_MODE_ID : AE_FULL_MANUAL\n");
    else if ( ret_val == AE_MANUAL_GAIN ) ret += sprintf(&buf[i],"AE_MODE_ID : AE_MANUAL_GAIN\n");
    else if ( ret_val == AE_MANUAL_EXPOSURE_TIME ) ret += sprintf(&buf[i],"AE_MODE_ID : AE_MANUAL_EXPOSURE_TIME\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AE_STATE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == AE_INACTIVE ) ret += sprintf(&buf[i],"AE_STATE_ID : AE_INACTIVE\n");
    else if ( ret_val == AE_SEARCHING ) ret += sprintf(&buf[i],"AE_STATE_ID : AE_SEARCHING\n");
    else if ( ret_val == AE_CONVERGED ) ret += sprintf(&buf[i],"AE_STATE_ID : AE_CONVERGED\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AE_SPLIT_PRESET_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == AE_SPLIT_BALANCED ) ret += sprintf(&buf[i],"AE_SPLIT_PRESET_ID : AE_SPLIT_BALANCED\n");
    else if ( ret_val == AE_SPLIT_INTEGRATION_PRIORITY ) ret += sprintf(&buf[i],"AE_SPLIT_PRESET_ID : AE_SPLIT_INTEGRATION_PRIORITY\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AE_GAIN_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"AE_GAIN_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AE_EXPOSURE_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"AE_EXPOSURE_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AE_ROI_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"AE_ROI_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AE_COMPENSATION_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"AE_COMPENSATION_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AWB_MODE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == AWB_AUTO ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_AUTO\n");
    else if ( ret_val == AWB_MANUAL ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_MANUAL\n");
    else if ( ret_val == AWB_DAY_LIGHT ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_DAY_LIGHT\n");
    else if ( ret_val == AWB_CLOUDY ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_CLOUDY\n");
    else if ( ret_val == AWB_INCANDESCENT ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_INCANDESCENT\n");
    else if ( ret_val == AWB_FLOURESCENT ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_FLOURESCENT\n");
    else if ( ret_val == AWB_TWILIGHT ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_TWILIGHT\n");
    else if ( ret_val == AWB_SHADE ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_SHADE\n");
    else if ( ret_val == AWB_WARM_FLOURESCENT ) ret += sprintf(&buf[i],"AWB_MODE_ID : AWB_WARM_FLOURESCENT\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AWB_STATE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == AWB_INACTIVE ) ret += sprintf(&buf[i],"AWB_STATE_ID : AWB_INACTIVE\n");
    else if ( ret_val == AWB_SEARCHING ) ret += sprintf(&buf[i],"AWB_STATE_ID : AWB_SEARCHING\n");
    else if ( ret_val == AWB_CONVERGED ) ret += sprintf(&buf[i],"AWB_STATE_ID : AWB_CONVERGED\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, AWB_TEMPERATURE_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"AWB_TEMPERATURE_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, ANTIFLICKER_MODE_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ANTIFLICKER_MODE_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, DEFOG_MODE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == DEFOG_DISABLE ) ret += sprintf(&buf[i],"DEFOG_MODE_ID : DEFOG_DISABLE\n");
    else if ( ret_val == DEFOG_ONLY ) ret += sprintf(&buf[i],"DEFOG_MODE_ID : DEFOG_ONLY\n");
    else if ( ret_val == DEFOG_BLEND ) ret += sprintf(&buf[i],"DEFOG_MODE_ID : DEFOG_BLEND\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, DEFOG_RATIO_DELTA, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"DEFOG_RATIO_DELTA : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, DEFOG_BLACK_PERCENTAGE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"DEFOG_BLACK_PERCENTAGE : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TALGORITHMS, DEFOG_WHITE_PERCENTAGE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"DEFOG_WHITE_PERCENTAGE : %d\n",ret_val);

    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TSELFTEST----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSELFTEST, FW_REVISION, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"FW_REVISION : %d\n",ret_val);

    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TREGISTERS----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TREGISTERS, REGISTERS_ADDRESS_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"REGISTERS_ADDRESS_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TREGISTERS, REGISTERS_SIZE_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"REGISTERS_SIZE_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TREGISTERS, REGISTERS_SOURCE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == SENSOR ) ret += sprintf(&buf[i],"REGISTERS_SOURCE_ID : SENSOR\n");
    else if ( ret_val == LENS ) ret += sprintf(&buf[i],"REGISTERS_SOURCE_ID : LENS\n");
    else if ( ret_val == ISP ) ret += sprintf(&buf[i],"REGISTERS_SOURCE_ID : ISP\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TREGISTERS, REGISTERS_VALUE_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"REGISTERS_VALUE_ID : %d\n",ret_val);

    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TSCENE_MODES----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSCENE_MODES, COLOR_MODE_ID, 0, COMMAND_GET, &ret_val );
    if ( ret_val == NORMAL ) ret += sprintf(&buf[i],"COLOR_MODE_ID : NORMAL\n");
    else if ( ret_val == BLACK_AND_WHITE ) ret += sprintf(&buf[i],"COLOR_MODE_ID : BLACK_AND_WHITE\n");
    else if ( ret_val == NEGATIVE ) ret += sprintf(&buf[i],"COLOR_MODE_ID : NEGATIVE\n");
    else if ( ret_val == SEPIA ) ret += sprintf(&buf[i],"COLOR_MODE_ID : SEPIA\n");
    else if ( ret_val == VIVID ) ret += sprintf(&buf[i],"COLOR_MODE_ID : VIVID\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSCENE_MODES, BRIGHTNESS_STRENGTH_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"BRIGHTNESS_STRENGTH_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSCENE_MODES, CONTRAST_STRENGTH_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"CONTRAST_STRENGTH_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSCENE_MODES, SATURATION_STRENGTH_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SATURATION_STRENGTH_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSCENE_MODES, SHARPENING_STRENGTH_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SHARPENING_STRENGTH_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSCENE_MODES, SHADING_STRENGTH_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"SHADING_STRENGTH_ID : %d\n",ret_val);

    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TSTATUS----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_EXPOSURE_LOG2_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_EXPOSURE_LOG2_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_GAIN_ONES_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_GAIN_ONES_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_GAIN_LOG2_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_GAIN_LOG2_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_AWB_MIX_LIGHT_CONTRAST, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_AWB_MIX_LIGHT_CONTRAST : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_AF_LENS_POS, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_AF_LENS_POS : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_AF_FOCUS_VALUE, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_AF_FOCUS_VALUE : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_EXPOSURE_CORRECTION_LOG2_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_EXPOSURE_CORRECTION_LOG2_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_TOTAL_GAIN_LOG2_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_TOTAL_GAIN_LOG2_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_TOTAL_GAIN_DB_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_TOTAL_GAIN_DB_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_CMOS_AGAIN_DB_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_CMOS_AGAIN_DB_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_CMOS_DGAIN_DB_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_CMOS_DGAIN_DB_ID : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TSTATUS, STATUS_INFO_ISP_DGAIN_DB_ID, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"STATUS_INFO_ISP_DGAIN_DB_ID : %d\n",ret_val);
#if 0
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n--------------TISP_MODULES----------------\n");
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_IRIDIX, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_IRIDIX : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SINTER, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_SINTER : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_TEMPER, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_TEMPER : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_AUTO_LEVEL, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_AUTO_LEVEL : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_FRAME_STITCH, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_FRAME_STITCH : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_RAW_FRONTEND, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_RAW_FRONTEND : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_BLACK_LEVEL, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_BLACK_LEVEL : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SHADING, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_SHADING : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_DEMOSAIC, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_DEMOSAIC : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_CNR, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_CNR : %d\n",ret_val);
    MOVE_I_TO_NEXT;
    acamera_command( ctx_id, TISP_MODULES, ISP_MODULES_MANUAL_SHARPEN, 0, COMMAND_GET, &ret_val );
    ret += sprintf(&buf[i],"ISP_MODULES_MANUAL_SHARPEN : %d\n",ret_val);
#endif
    return ret;
}
static ssize_t isp_proc_write(
    struct device *dev, struct device_attribute *attr,
    char const *buf, size_t size)
{
    return size;
}

static DEVICE_ATTR(isp_proc, S_IRUGO | S_IWUSR, isp_proc_read, isp_proc_write);

void system_isp_proc_create( struct device *dev )
{
    device_create_file(dev, &dev_attr_isp_proc);
}

void system_isp_proc_remove( struct device *dev )
{
    device_remove_file(dev, &dev_attr_isp_proc);
}

#if 0
static ssize_t isp_stats_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
    ssize_t ret = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t * awb_stats_info = (uint32_t *)kmalloc(sizeof(uint32_t)*(ISP_METERING_ZONES_AWB_V*ISP_METERING_ZONES_AWB_H+20)*2, GFP_KERNEL);
    uint32_t ctx_id = acamera_get_api_context();
    ret += sprintf(buf,"\n--------------AWB_stats----------------\n");
    acamera_command( ctx_id, TSTATUS, AWB_STATS_ID, 0, COMMAND_GET, awb_stats_info );
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"size : %d sum : %d\n",awb_stats_info[0],awb_stats_info[1]);

    for (j = 0;j < awb_stats_info[0]*2; j+=2) {
        MOVE_I_TO_NEXT;
        ret += sprintf(&buf[i],"%x ",awb_stats_info[j+2]);
        if ( (j % 33) == 0) {
            MOVE_I_TO_NEXT;
            ret += sprintf(&buf[i],"\n");
        }
    }
    MOVE_I_TO_NEXT;
    ret += sprintf(&buf[i],"\n");
    kfree(awb_stats_info);
    return ret;

}

static DEVICE_ATTR(isp_stats, S_IRUGO | S_IWUSR, isp_stats_read, NULL);

void system_isp_stats_create( struct device *dev )
{
    device_create_file(dev, &dev_attr_isp_stats);
}

void system_isp_stats_remove( struct device *dev )
{
    device_remove_file(dev, &dev_attr_isp_stats);
}
#endif


