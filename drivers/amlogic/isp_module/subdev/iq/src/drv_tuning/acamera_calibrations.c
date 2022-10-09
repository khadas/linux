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

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "acamera_firmware_config.h"
#include "acamera_logger.h"
#include "acamera_calibrations.h"
#include "system_timer.h"
#include <linux/module.h>
#include "acamera_get_calibrations_dummy.h"

LookupTable *_GET_LOOKUP_PTR( ACameraCalibrations *p_ctx, uint32_t idx )
{
    LookupTable *result = NULL;
    if ( idx < CALIBRATION_TOTAL_SIZE ) {
        result = p_ctx->calibrations[idx];
    } else {
        result = NULL;
        LOG( LOG_CRIT, "Trying to access an isp lut with invalid index %d", (int)idx );
    }
    return result;
}

const void *_GET_LUT_PTR( ACameraCalibrations *p_ctx, uint32_t idx )
{
    const void *result = NULL;
    LookupTable *lut = _GET_LOOKUP_PTR( p_ctx, idx );
    if ( lut != NULL ) {
        result = lut->ptr;
    }

    return result;
}

// use fast version of lut access routines
uint8_t *_GET_UCHAR_PTR( ACameraCalibrations *p_ctx, uint32_t idx )
{
    uint8_t *result = (uint8_t *)_GET_LUT_PTR( p_ctx, idx );
    return result;
}

uint16_t *_GET_USHORT_PTR( ACameraCalibrations *p_ctx, uint32_t idx )
{
    uint16_t *result = (uint16_t *)_GET_LUT_PTR( p_ctx, idx );
    return result;
}

uint32_t *_GET_UINT_PTR( ACameraCalibrations *p_ctx, uint32_t idx )
{
    uint32_t *result = (uint32_t *)_GET_LUT_PTR( p_ctx, idx );
    return result;
}

modulation_entry_t *_GET_MOD_ENTRY16_PTR( ACameraCalibrations *p_ctx, uint32_t idx )
{
    modulation_entry_t *result = (modulation_entry_t *)_GET_LUT_PTR( p_ctx, idx );
    return result;
}

modulation_entry_32_t *_GET_MOD_ENTRY32_PTR( ACameraCalibrations *p_ctx, uint32_t idx )
{
    modulation_entry_32_t *result = (modulation_entry_32_t *)_GET_LUT_PTR( p_ctx, idx );
    return result;
}

uint32_t _GET_ROWS( ACameraCalibrations *p_ctx, uint32_t idx )
{
    uint32_t result = 0;
    LookupTable *lut = _GET_LOOKUP_PTR( p_ctx, idx );
    if ( lut != NULL ) {
        result = lut->rows;
    }
    return result;
}

uint32_t _GET_COLS( ACameraCalibrations *p_ctx, uint32_t idx )
{
    uint32_t result = 0;
    LookupTable *lut = _GET_LOOKUP_PTR( p_ctx, idx );
    if ( lut != NULL ) {
        result = lut->cols;
    }
    return result;
}

uint32_t _GET_LEN( ACameraCalibrations *p_ctx, uint32_t idx )
{
    uint32_t result = 0;
    LookupTable *lut = _GET_LOOKUP_PTR( p_ctx, idx );
    if ( lut != NULL ) {
        result = lut->cols * lut->rows;
    }
    return result;
}

uint32_t _GET_WIDTH( ACameraCalibrations *p_ctx, uint32_t idx )
{
    uint32_t result = 0;
    LookupTable *lut = _GET_LOOKUP_PTR( p_ctx, idx );
    if ( lut != NULL ) {
        result = lut->width;
    }
    return result;
}

uint32_t _GET_SIZE( ACameraCalibrations *p_ctx, uint32_t idx )
{
    uint32_t result = 0;
    LookupTable *lut = _GET_LOOKUP_PTR( p_ctx, idx );
    if ( lut != NULL ) {
        result = lut->cols * lut->rows * lut->width;
    }
    return result;
}

static void acamera_load_external_bin(ACameraCalibrations *c, char *sensor_name, int32_t preset, int32_t mode)
{
    struct file *fp_s = NULL,*fp_d = NULL;
    mm_segment_t fs;
    int32_t nread = 0;
    uint32_t idx = 0;
    uint32_t l_size = 0;
    uint8_t *l_ptr = NULL;
    uint32_t t_size = 0;
    uint32_t i = 0;
    uint8_t *b_buf = NULL;
    uint8_t *p_mem = NULL;
    loff_t pos = 0;

    struct kstat stat;
    uint32_t s_size = 0, d_size = 0;

    for (idx = 0; idx < CALIBRATION_TOTAL_SIZE; idx++) {
        l_size = _GET_SIZE(c, idx);
        if (l_size)
             l_size += 8;
        t_size += l_size;
    }

    fs = get_fs();
    set_fs(KERNEL_DS);

    char static_name[70] = {'\0'};
    char dynamic_name[70] = {'\0'};

    if (sensor_name) {
        if (mode == 0) {
            if (preset == WDR_MODE_FS_LIN) {
                sprintf(static_name, "/data/isp_tuning/%s-static-fs-lin.bin", sensor_name);
                sprintf(dynamic_name, "/data/isp_tuning/%s-dynamic-fs-lin.bin", sensor_name);
            } else {
                sprintf(static_name, "/data/isp_tuning/%s-static-linear.bin", sensor_name);
                sprintf(dynamic_name, "/data/isp_tuning/%s-dynamic-linear.bin", sensor_name);
            }
        } else {
            if (preset == WDR_MODE_FS_LIN) {
                sprintf(static_name, "/data/isp_tuning/%s-static-fs-lin-%d.bin", sensor_name, mode);
                sprintf(dynamic_name, "/data/isp_tuning/%s-dynamic-fs-lin-%d.bin", sensor_name, mode);
            } else {
                sprintf(static_name, "/data/isp_tuning/%s-static-linear-%d.bin", sensor_name, mode);
                sprintf(dynamic_name, "/data/isp_tuning/%s-dynamic-linear-%d.bin", sensor_name, mode);
            }
        }
    } else {
        if (mode == 0) {
            if (preset == WDR_MODE_FS_LIN) {
                sprintf(static_name, "/data/isp_tuning/static-fs-lin.bin");
                sprintf(dynamic_name, "/data/isp_tuning/dynamic-fs-lin.bin");
            } else {
                sprintf(static_name, "/data/isp_tuning/static-linear.bin");
                sprintf(dynamic_name, "/data/isp_tuning/dynamic-linear.bin");
            }
        } else {
            if (preset == WDR_MODE_FS_LIN) {
                sprintf(static_name, "/data/isp_tuning/static-fs-lin-%d.bin", mode);
                sprintf(dynamic_name, "/data/isp_tuning/dynamic-fs-lin-%d.bin", mode);
            } else {
                sprintf(static_name, "/data/isp_tuning/static-linear-%d.bin", mode);
                sprintf(dynamic_name, "/data/isp_tuning/dynamic-linear-%d.bin", mode);
            }
        }
    }

    if (vfs_stat(static_name, &stat)) {
        goto error_size;
    }
    s_size = stat.size - 4;

    if (vfs_stat(dynamic_name, &stat)) {
        goto error_size;
    }
    d_size = stat.size - 4;

    LOG(LOG_INFO, "Bin size match: s_size %u, d_size %u, total:%u, t_size %u", s_size, d_size, s_size+d_size, t_size);

    if (t_size < (s_size + d_size)) {
        LOG(LOG_CRIT, "Bin size not match: s_size %u, d_size %u, t_size %u", s_size, d_size, t_size);
        goto error_size;
    }

    fp_s = filp_open(static_name, O_RDONLY, 0);
    fp_d = filp_open(dynamic_name, O_RDONLY, 0);
    if (fp_s == NULL || fp_d == NULL)
        goto error_open;

    b_buf = kzalloc((s_size + d_size), GFP_KERNEL);
    if (b_buf == NULL) {
        LOG(LOG_ERR, "Failed to alloc mem");
        goto error_open;
    }

    nread = vfs_read(fp_s, b_buf, s_size, &pos);
    pos = 0;
    nread += vfs_read(fp_d, b_buf + s_size, d_size, &pos);
    if (nread > t_size) {
        LOG(LOG_CRIT, "Failed to read bin");
        goto error_read;
    }

    for (i = 0; i < nread; i++) {
        if ( *(uint32_t *)(b_buf + i) == 0x11223344 ) {
            idx = *(uint16_t *)(b_buf + i + 4);
            p_mem = b_buf + i + 8;
            l_ptr = (uint8_t *)_GET_LUT_PTR(c, idx);
            l_size = _GET_SIZE(c, idx);
            if (l_size)
                memcpy(l_ptr, p_mem, l_size);
            else
                LOG(LOG_INFO, "LUT %d size is null", idx);
        }
    }

error_read:
    if (b_buf)
        kfree(b_buf);

error_open:
    if (fp_s)
        filp_close(fp_s, NULL);
    if (fp_d)
        filp_close(fp_d, NULL);

error_size:
    set_fs(fs);
}

uint32_t get_calibratin_external_tuning( char *sensor_name, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;

    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;
    int32_t mode = ( (sensor_mode_t *)sensor_arg )->cali_mode;

    acamera_load_external_bin(c, sensor_name, preset, mode);

    return ret;
}


