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

#ifndef __ACAMERA_SENSOR_API_H__
#define __ACAMERA_SENSOR_API_H__

#include "acamera_types.h"


// this structure represents image resolution
// it is used in sensor driver to keep information
// about the frame width and frame height
typedef struct _image_resolution_t {
    uint16_t width;
    uint16_t height;
} image_resolution_t;


// a sensor can support several different predefined modes.
// this structure keeps all neccessary information about a mode
typedef struct _sensor_mode_t {
    uint8_t wdr_mode;              // The wdr mode.
    uint32_t fps;                  // Fps value multiplied by 256
    image_resolution_t resolution; // Resolution of the mode
    uint8_t exposures;             // How many exposures this mode supports
    uint8_t bits;                  // Bit depth of data from sensor
} sensor_mode_t;


// sensor parameters structure keeps information about the current
// sensor state.
typedef struct _sensor_param_t {
    image_resolution_t total;                // Total resolution of the image with blanking
    image_resolution_t active;               // Active resolution without blanking
    uint16_t pixels_per_line;                // Actual pixels per line after scaling/binning
    int32_t again_log2_max;                  // Maximum analog gain value in log2 format
    int32_t dgain_log2_max;                  // Maximum digital gain value in log2 format
    int32_t again_accuracy;                  // Precision of the gain - If required gain step is less then this do not try to allocate it
    uint32_t integration_time_min;           // Minimum integration time for the sensor in lines
    uint32_t integration_time_max;           // Maximum integration time for the sensor in lines without dropping fps
    uint32_t integration_time_long_max;      // Maximum integration time for long
    uint32_t integration_time_limit;         // Maximum possible integration time for the sensor
    uint16_t day_light_integration_time_max; // Limit of integration time for non-flickering light source
    uint8_t integration_time_apply_delay;    // Delay to apply integration time in frames
    uint8_t isp_exposure_channel_delay;      // Select which WDR exposure channel gain is delayed 0-none, 1-long, 2-medium, 3-short (only 0 and 1 implemented)
    int32_t xoffset;                         // Used for image stabilization
    int32_t yoffset;                         // Used for image stabilization
    uint32_t lines_per_second;               // Number of lines per second used for antiflicker
    int32_t sensor_exp_number;               // Number of different exposures supported by the sensor
    sensor_mode_t *modes_table;              // Table of predefined modes which are supported by the sensor
    uint32_t modes_num;                      // The number of predefined modes
    uint8_t mode;                            // Current mode. This value is from the range [ 0 : modes_num - 1 ]
    void *sensor_ctx;                        // Conext to a sensor structure. This structure is not available to firmware
} sensor_param_t;


// sensor control structure implements sensor API which is used by firmware
typedef struct _sensor_control_t {
    /**
     *   Allocate analog gain
     *
     *   This function sets the sensor analog gain.
     *   Gain should be just saved here for the future.
     *   The real sensor analog gain update must be implemented in
     *   sensor_update routine.
     *
     *   @param gain - analog gain value in log2 format precision is defined by LOG2_GAIN_SHIFT
     *          ctx - pointer to the sensor context
     *
     *   @return the real analog gain which will be applied
     */
    int32_t ( *alloc_analog_gain )( void *ctx, int32_t gain );


    /**
     *   Allocate digital gain
     *
     *   This function sets the sensor digital gain.
     *   Gain should be just saved here for the future.
     *   The real sensor digital gain update must be implemented in
     *   sensor_update routine.
     *
     *   @param gain - analog gain value in log2 format precision is defined by LOG2_GAIN_SHIFT
     *          ctx - pointer to the sensor context
     *
     *   @return the real digital gain which will be applied
     */
    int32_t ( *alloc_digital_gain )( void *ctx, int32_t gain );


    /**
     *   Allocate integration time
     *
     *   This function sets the sensor integration time.
     *   Integration time should be just saved here for the future.
     *   The real time update must be implemented in
     *   sensor_update routine.
     *
     *   @param int_time - integration time if one exposure is used or short exposure for multi-expsoure sensors.
     *          int_time_M - medium integration time if several exposures are supported.
     *          int_time_L - long integration time if several exposures are supported.
     *          ctx - pointer to the sensor context
     *
     */
    void ( *alloc_integration_time )( void *ctx, uint16_t *int_time, uint16_t *int_time_M, uint16_t *int_time_L );


    /**
     *   Update all sensor parameters
     *
     *   The function is called from IRQ thread in vertical blanking.
     *   All sensor parameters must be updated here.
     *   @param ctx - pointer to the sensor context
     *
     */
    void ( *sensor_update )( void *ctx );


    /**
     *   Set horizontal offset
     *
     *   Set sensor horizontal offset to implement crop funcionality.
     *
     *   @param ctx - pointer to the sensor context
     *
     *   @return amount of offseted pixels
     */
    uint32_t ( *set_xoffset )( void *ctx, uint32_t xoffset );


    /**
     *   Set vertical offset
     *
     *   Set sensor vertical offset to implement crop funcionality.
     *
     *   @param ctx - pointer to the sensor context
     *
     *   @return amount of offseted pixels
     */
    uint32_t ( *set_yoffset )( void *ctx, uint32_t xoffset );


    /**
     *   Set the sensor mode
     *
     *   Sensor can support several modes. This function
     *   is used to switch among them.
     *
     *   @param mode - the new mode to set
     *          ctx - pointer to the sensor context
     *
     */
    void ( *set_mode )( void *ctx, uint8_t mode );


    /**
     *   Start sensor data output
     *
     *   This function is called from system to
     *   enable video stream from sensor.
     *
     *   @param ctx - pointer to the sensor context
     *
     */
    void ( *start_streaming )( void *ctx );


    /**
     *   Stop sensor data output
     *
     *   This function is called from system to
     *   disable video stream from sensor.
     *
     *   @param ctx - pointer to the sensor context
     *
     */
    void ( *stop_streaming )( void *ctx );


    /**
     *   Get sensor id
     *
     *   This function returns sensor id if sensor has this option; if not -1 is returned
     *
     *   @param ctx - pointer to the sensor context
     */
    uint16_t ( *get_id )( void *ctx );


    /**
     *   Get sensor parameters
     *
     *   This function returns a pointer to a sensor parameter structure
     *
     *   @param ctx - pointer to the sensor context
     */
    const sensor_param_t *( *get_parameters )( void *ctx );


    /**
     *   disable on-sensor isp
     *
     *   @param ctx - pointer to the sensor context
     */
    void ( *disable_sensor_isp )( void *ctx );


    /**
     *   read on-sensor register
     *
     *   @param ctx - pointer to the sensor context
     *      address - address of register
     */
    uint32_t ( *read_sensor_register )( void *ctx, uint32_t address );


    /**
     *   write on-sensor register
     *
     *   @param ctx - pointer to the sensor context
     *      address - address of register
     *         data - data to write to register location
     */
    void ( *write_sensor_register )( void *ctx, uint32_t address, uint32_t data );

} sensor_control_t;


typedef sensor_control_t *sensor_control_ptr_t;


#endif /* __ACAMERA_SENSOR_API_H__ */
