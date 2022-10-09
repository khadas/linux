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
#include "acamera_firmware_config.h"


extern void sensor_init_ov08a10( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_ov08a10( void *ctx );
extern int sensor_detect_ov08a10( void* sbp);

extern void sensor_init_ov04a10( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_ov04a10( void *ctx );
extern int sensor_detect_ov04a10( void* sbp);


extern void sensor_init_imx290( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx290( void *ctx );
extern int sensor_detect_imx290( void* sbp);

extern void sensor_init_imx290sub( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx290sub( void *ctx );
extern int sensor_detect_imx290sub( void* sbp);

extern void sensor_init_imx290ssub( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx290ssub( void *ctx );
extern int sensor_detect_imx290ssub( void* sbp);

extern void sensor_init_imx290sssub( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx290sssub( void *ctx );
extern int sensor_detect_imx290sssub( void* sbp);

extern void sensor_init_imx227( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx227( void *ctx );
extern int sensor_detect_imx227( void* sbp);

extern void sensor_init_imx481( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx481( void *ctx );
extern int sensor_detect_imx481( void* sbp);

extern void sensor_init_imx307( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx307( void *ctx );
extern int sensor_detect_imx307( void* sbp);

extern void sensor_init_imx224( void** ctx, sensor_control_t*, void*);
extern void sensor_deinit_imx224( void *ctx );
extern int sensor_detect_imx224( void* sbp);

extern void sensor_init_imx335( void** ctx, sensor_control_t*, void*);
extern void sensor_deinit_imx335( void *ctx );
extern int sensor_detect_imx335( void* sbp);

extern void sensor_init_imx415( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx415( void *ctx );
extern int sensor_detect_imx415( void* sbp);

extern void sensor_init_ov13858( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_ov13858( void *ctx );
extern int sensor_detect_ov13858( void* sbp);

extern void sensor_init_sc2232h( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_sc2232h( void *ctx );
extern int sensor_detect_sc2232h( void* sbp );

extern void sensor_init_sc4238( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_sc4238( void *ctx );
extern int sensor_detect_sc4238( void* sbp);

extern void sensor_init_sc2335( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_sc2335( void *ctx );
extern int sensor_detect_sc2335( void* sbp);

extern void sensor_init_imx334( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx334( void *ctx );
extern int sensor_detect_imx334( void* sbp);

extern void sensor_init_sc8238cs( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_sc8238cs( void *ctx );
extern int sensor_detect_sc8238cs( void* sbp);

extern void sensor_init_ov5640( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_ov5640( void *ctx );
extern int sensor_detect_ov5640( void* sbp);

extern void sensor_init_ov5640sub( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_ov5640sub( void *ctx );
extern int sensor_detect_ov5640sub( void* sbp);

extern void sensor_init_ov5675( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_ov5675( void *ctx );
extern int sensor_detect_ov5675( void* sbp);

extern void sensor_init_virtcam( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_virtcam( void *ctx );
extern int sensor_detect_virtcam( void* sbp);

extern void sensor_init_s5k3m5( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_s5k3m5( void *ctx );
extern int sensor_detect_s5k3m5( void* sbp);

extern void sensor_init_lt6911( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_lt6911( void *ctx );
extern int sensor_detect_lt6911( void* sbp);

#define SENSOR_INIT_SUBDEV_FUNCTIONS_OS08A10 sensor_init_ov08a10
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_OS08A10 sensor_deinit_ov08a10
#define SENSOR_DETECT_FUNCTIONS_OS08A10 sensor_detect_ov08a10

#define SENSOR_INIT_SUBDEV_FUNCTIONS_S5K3M5 sensor_init_s5k3m5
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_S5K3M5 sensor_deinit_s5k3m5
#define SENSOR_DETECT_FUNCTIONS_S5K3M5 sensor_detect_s5k3m5

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290 sensor_init_imx290
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290 sensor_deinit_imx290
#define SENSOR_DETECT_FUNCTIONS_IMX290 sensor_detect_imx290

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SUB sensor_init_imx290sub
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SUB sensor_deinit_imx290sub
#define SENSOR_DETECT_FUNCTIONS_IMX290SUB sensor_detect_imx290sub

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SSUB sensor_init_imx290ssub
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SSUB sensor_deinit_imx290ssub
#define SENSOR_DETECT_FUNCTIONS_IMX290SSUB sensor_detect_imx290ssub

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290SSSUB sensor_init_imx290sssub
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290SSSUB sensor_deinit_imx290sssub
#define SENSOR_DETECT_FUNCTIONS_IMX290SSSUB sensor_detect_imx290sssub

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX227 sensor_init_imx227
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX227 sensor_deinit_imx227
#define SENSOR_DETECT_FUNCTIONS_IMX227 sensor_detect_imx227

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX307 sensor_init_imx307
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX307 sensor_deinit_imx307
#define SENSOR_DETECT_FUNCTIONS_IMX307 sensor_detect_imx307

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX224 sensor_init_imx224
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX224 sensor_deinit_imx224
#define SENSOR_DETECT_FUNCTIONS_IMX224 sensor_detect_imx224

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX335 sensor_init_imx335
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX335 sensor_deinit_imx335
#define SENSOR_DETECT_FUNCTIONS_IMX335 sensor_detect_imx335

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX415 sensor_init_imx415
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX415 sensor_deinit_imx415
#define SENSOR_DETECT_FUNCTIONS_IMX415 sensor_detect_imx415

#define SENSOR_INIT_SUBDEV_FUNCTIONS_OV13858 sensor_init_ov13858
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_OV13858 sensor_deinit_ov13858
#define SENSOR_DETECT_FUNCTIONS_OV13858 sensor_detect_ov13858

#define SENSOR_INIT_SUBDEV_FUNCTIONS_SC2232H sensor_init_sc2232h
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_SC2232H sensor_deinit_sc2232h
#define SENSOR_DETECT_FUNCTIONS_SC2232H sensor_detect_sc2232h

#define SENSOR_INIT_SUBDEV_FUNCTIONS_SC4238 sensor_init_sc4238
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_SC4238 sensor_deinit_sc4238
#define SENSOR_DETECT_FUNCTIONS_SC4238 sensor_detect_sc4238

#define SENSOR_INIT_SUBDEV_FUNCTIONS_SC2335 sensor_init_sc2335
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_SC2335 sensor_deinit_sc2335
#define SENSOR_DETECT_FUNCTIONS_SC2335 sensor_detect_sc2335

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX334 sensor_init_imx334
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX334 sensor_deinit_imx334
#define SENSOR_DETECT_FUNCTIONS_IMX334 sensor_detect_imx334

#define SENSOR_INIT_SUBDEV_FUNCTIONS_OV5640 sensor_init_ov5640
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_OV5640 sensor_deinit_ov5640
#define SENSOR_DETECT_FUNCTIONS_OV5640 sensor_detect_ov5640

#define SENSOR_INIT_SUBDEV_FUNCTIONS_OV5640SUB sensor_init_ov5640sub
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_OV5640SUB sensor_deinit_ov5640sub
#define SENSOR_DETECT_FUNCTIONS_OV5640SUB sensor_detect_ov5640sub

#define SENSOR_INIT_SUBDEV_FUNCTIONS_OV5675 sensor_init_ov5675
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_OV5675 sensor_deinit_ov5675
#define SENSOR_DETECT_FUNCTIONS_OV5675 sensor_detect_ov5675

#define SENSOR_INIT_SUBDEV_FUNCTIONS_LT6911 sensor_init_lt6911
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_LT6911 sensor_deinit_lt6911
#define SENSOR_DETECT_FUNCTIONS_LT6911 sensor_detect_lt6911

#define SENSOR_INIT_SUBDEV_FUNCTIONS_VIRTCAM sensor_init_virtcam
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_VIRTCAM sensor_deinit_virtcam
#define SENSOR_DETECT_FUNCTIONS_VIRTCAM sensor_detect_virtcam

