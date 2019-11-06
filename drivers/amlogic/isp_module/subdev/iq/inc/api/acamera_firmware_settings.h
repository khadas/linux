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

#ifndef __ACAMERA_FIRMWARE_SETTNGS_H__
#define __ACAMERA_FIRMWARE_SETTNGS_H__


#include "acamera_sensor_api.h"

// formats which are supported on output of ISP
// they can be used to set a desired output format
// in acamera_settings structure
#define DMA_FORMAT_DISABLE      0
#define DMA_FORMAT_RGB32        1
#define DMA_FORMAT_A2R10G10B10  2
#define DMA_FORMAT_RGB565       3
#define DMA_FORMAT_RGB24        4
#define DMA_FORMAT_GEN32        5
#define DMA_FORMAT_RAW16        6
#define DMA_FORMAT_RAW12        7
#define DMA_FORMAT_AYUV         8
#define DMA_FORMAT_Y410         9
#define DMA_FORMAT_YUY2         10
#define DMA_FORMAT_UYVY         11
#define DMA_FORMAT_Y210         12
#define DMA_FORMAT_NV12_Y       13
#define DMA_FORMAT_NV12_UV      (1<<6|13)
#define DMA_FORMAT_NV12_VU      (2<<6|13)
#define DMA_FORMAT_YV12_Y       14
#define DMA_FORMAT_YV12_U       (1<<6|14)
#define DMA_FORMAT_YV12_V       (2<<6|14)


static inline uint32_t _get_pixel_width( uint32_t format )
{
  uint32_t result = 4 ;
  switch(format)
  {

  case DMA_FORMAT_RGB565:
  case DMA_FORMAT_RAW16:
  case DMA_FORMAT_YUY2:
  case DMA_FORMAT_UYVY:
  case DMA_FORMAT_RAW12:
    result = 2;
    break;
  case DMA_FORMAT_NV12_Y:
  case DMA_FORMAT_NV12_UV:
  case DMA_FORMAT_NV12_VU:
  case DMA_FORMAT_YV12_Y:
  case DMA_FORMAT_YV12_U:
  case DMA_FORMAT_YV12_V:
    result = 1;
    break;
  }
  return result ;
}


// calibration table.
typedef struct _ACameraCalibrations {
    LookupTable* calibrations[ CALIBRATION_TOTAL_SIZE ] ;
} ACameraCalibrations ;



typedef struct _aframe_t {
    uint32_t type ;        // frame type.
    uint32_t width ;       // frame width
    uint32_t height ;      // frame height
    uint32_t address ;     // start of memory block
    uint32_t line_offset ; // line offset for the frame
    uint32_t size ; // total size of the memory in bytes
    uint32_t status ;
} aframe_t ;

typedef struct _tframe_t {
    aframe_t primary;        //primary frames
    aframe_t secondary ;        //secondary frames
} tframe_t ;


// this structure is used by firmware to return the information
// about an output frame
typedef struct _metadata_t {
    uint8_t format;
    uint16_t width;
    uint16_t height;
    uint32_t frame_number;
    uint16_t line_size;
    uint32_t exposure;
    uint32_t int_time;
    uint32_t int_time_medium;
    uint32_t int_time_long;
    uint32_t again;
    uint32_t dgain;
    uint32_t addr;
    uint32_t isp_dgain;
    int8_t dis_offset_x;
    int8_t dis_offset_y;
    uint32_t frame_id;
} metadata_t ;


typedef struct _acamera_settings {
    void (*sensor_init)( void** ctx, sensor_control_t* ctrl) ;         // must be initialized to provide sensor initialization entry. Must be provided.
    void (*sensor_deinit)( void *ctx ) ;         // must be initialized to provide sensor initialization entry. Must be provided.
    uint32_t (*get_calibrations)( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations * ) ;  // must be initialized to provide calibrations. Must be provided.
    void (*custom_initialization)( uint32_t ctx_num ) ;                       // customer initialization sequence. called at the end of acamera_init. May be NULL.
    uintptr_t isp_base ;                                                 // isp base offset (not absolute memory address ). Should be started from start of isp memory. All ISP r/w accesses inside the firmware will use this value as the start_offset.
    aframe_t* temper_frames ;                                          // frames to be used for the front-end dma writer
    uint32_t  temper_frames_number ;                                   // number of frames for dma front-end
    tframe_t* fr_frames ;                                              // frames to be used for the fr output dma writer
    uint32_t  fr_frames_number ;                                       // number of frames for frame fr pipe
    void (*callback_fr)( uint32_t ctx_num, tframe_t * tframe, const metadata_t *metadata ) ;  // callback on every FR output frame. can be null if there is no fr output
    tframe_t* ds1_frames ;                                              // frames to be used for the ds1 dma writer
    uint32_t  ds1_frames_number ;                                       // number of frames for ds1 pipe
    void (*callback_ds1)( uint32_t ctx_num, tframe_t * tframe, const metadata_t *metadata ) ; // callback on every DS1 output frame. can be null if there is no ds1 output
} acamera_settings ;

#endif
