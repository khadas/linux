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

#ifndef __SOC_SENSOR_H__
#define __SOC_SENSOR_H__

// This name is used by both V4L2 ISP device and
// V4L2 sensor subdevice to match the subdevice in the
// V4L2 async subdevice list.
#define V4L2_SOC_SENSOR_NAME "SocSensor"

// This is used as the main communication structure between
// V4L2 ISP Device and V4L2 Sensor Subdevice
// Parameters are used differently depending on the actual API command ID.
struct soc_sensor_ioctl_args {
    uint32_t ctx_num;
    union {
        // This struct is used to set the new integration time parameters
        // for the sensor.
        // The Interface supports up to 4 different exposures for HDR scenes.
        // This structure is used only for SOC_SENSOR_ALLOC_IT API call.
        struct {
            uint16_t it_short;  // short integration time
            uint16_t it_long;   // long integration time
            uint16_t it_medium; // medium integration time
        } integration_time;
        // This struct is used for all API commands except SOC_SENSOR_ALLOC_IT
        // It contains some general parameters the meaning of which is different
        // and depends on the specific API ID.
        struct {
            uint32_t val_in;  // first input value
            uint32_t val_in2; // second input value
            uint32_t val_out; // output value
        } general;
    } args;
    sensor_name_t s_name;
    isp_context_seq isp_context_seq;
};

// The enum declares the API commands ID which
// must be supported by V4L2 sensor subdevice.
// This API ID will be used on each ioctl call from
// V4L2 ISP device
enum SocCamera_ioctl {
    //########## CONTROLS ###########//

    // Enable sensor data streaming
    // input: none
    // output: none
    SOC_SENSOR_STREAMING_ON = 0,

    // disable sensor data streaming
    // input: none
    // output: none
    SOC_SENSOR_STREAMING_OFF,


    // set a new sensor preset
    // Each sensor driver can support any number of different presets. A preset is a combination
    // of a resolution, fps and wdr mode. V4L2 ISP driver should know in advance about the presets
    // used and will use them by number from 0 till PR_NUM-1.
    //
    // input: val_in: change sensor preset
    // output: none
    SOC_SENSOR_SET_PRESET,

    // allocate new analog gain.
    // The V4L2 ISP device will try to set a new analog gain on every frame
    // V4L2 sensor subdevice should save the closest possible value which is less
    // or equal then requested one.
    // The saved value must be applied on the time when SOC_SENSOR_UPDATE_EXP is called.
    // The meaning of this command is to get the real possible analog gain
    // based on sensor driver limitations.
    // The returned value will be used to adjust other gains if requested value cannot be
    // used exactly.
    //
    // input: val_in: requested analog gain
    // output: val_out: actual analog gain value which can be used by the sensor.
    SOC_SENSOR_ALLOC_AGAIN,

    // allocate new digital gain.
    // The V4L2 ISP device will try to set a new digital gain on every frame
    // V4L2 sensor subdevice should save the closest possible value which is less
    // or equal then requested one.
    // This saved value must be applied on the time when SOC_SENSOR_UPDATE_EXP is called.
    // The meaning of this command is to get the real possible dital gain
    // based on sensor driver limitations.
    // The returned value will be used to adjust other gains if requested value cannot be
    // used exactly.
    //
    // input: val_in: requested digital gain
    // output: val_out: actual digital gain value which can be used by the sensor.
    SOC_SENSOR_ALLOC_DGAIN,

    // allocate new integration time
    // The V4L2 ISP device will try to set a new integration time on every frame
    // That is the only command which uses integration_time structure as input
    // parameter for ioctl call.
    // Integration time is a combination of 1, 2 or 3 different integration times.
    // The number depends on the current sensor preset. For example if the sensor
    // is working in linear mode only it_short is used. If sensor is initialized
    // in DOL 3Exp mode all it_short, it_medium and it_long will be send.
    //
    // input: it_short: integration time for short exposure ( should be used for linear mode)
    // input: it_medium: integration time for medium exposure
    // input: it_long: integration time for long exposure
    // output: none
    SOC_SENSOR_ALLOC_IT,

    // The function is called every frame.
    // All previously set parameters for Analog/Digital gain and Integration time
    // must be send to the sensor on this call at the same time.
    // input: none
    // output: none
    SOC_SENSOR_UPDATE_EXP,

    // read sensor register value
    // input: val_in - register address
    // output: val_out - register value
    SOC_SENSOR_READ_REG,

    // write a value to the sensor register
    // input: val_in - register address
    // input: val_in2 - register value
    // output: none
    SOC_SENSOR_WRITE_REG,


    //########## STATIC PARAMETERS ###########//

    // Return the number of supported presets.
    // This call is used by V4L2 ISP device to understand how many
    // and what kind of presets are supported by the sensor driver
    // input: none
    // output: val_out - number of supported presets. Minimum 1
    SOC_SENSOR_GET_PRESET_NUM,

    // return current preset
    // input: none
    // output: val_out - preset number
    SOC_SENSOR_GET_PRESET_CUR,

    // Get a sensor image widht for a given preset
    // input: val_in - preset number
    // output: val_out - image width for a given preset
    SOC_SENSOR_GET_PRESET_WIDTH,

    // Get a sensor image height for a given preset
    // input: val_in - preset number
    // output: val_out - image height for a given preset
    SOC_SENSOR_GET_PRESET_HEIGHT,

    // Get a sensor fps for a given preset
    // input: val_in - preset number
    // output: val_out - fps for a given preset
    SOC_SENSOR_GET_PRESET_FPS,

    // get exposure numbers for a given preset
    // input: val_in - preset number
    // output: val_out - number of exposures
    SOC_SENSOR_GET_PRESET_EXP,

    // Get a sensor mode for a given preset
    // input: val_in - preset number
    // output: val_out - WDR_MODE_LINEAR or WDR_MODE_FS_LIN (DOL)
    SOC_SENSOR_GET_PRESET_MODE,


    //########## DYNAMIC PARAMETERS ###########//

    // return current number of different exposures
    // This command should return actual number of different
    // exposures from the sensor
    // input: none
    // output: val_out - number of exposures. Min 1, Max is number of exposure supported by the sensor.
    SOC_SENSOR_GET_EXP_NUMBER,

    // return maximum integration time in lines for the current mode
    // input: none
    // output: val_out - maximum integration time in lines
    SOC_SENSOR_GET_INTEGRATION_TIME_MAX,

    // return maximum integration time for the long exposure
    // input: none
    // output: val_out - maximum long integration time
    SOC_SENSOR_GET_INTEGRATION_TIME_LONG_MAX,

    // return current minimum integration time in lines
    // input: none
    // output: val_out - min it in lines
    SOC_SENSOR_GET_INTEGRATION_TIME_MIN,

    // return current maximum integration time limit
    // input: none
    // output: val_out - maximum limit for it.
    SOC_SENSOR_GET_INTEGRATION_TIME_LIMIT,

    // return current maximum possible analog gain value
    // The returned value must be in log2 format with LOG2_GAIN_SHIFT bits precesion.
    // input: none
    // output: val_out - maximum analog gain
    SOC_SENSOR_GET_ANALOG_GAIN_MAX,

    // return current maximum possible digital gain value
    // The returned value must be in log2 format with LOG2_GAIN_SHIFT bits precesion.
    // input: none
    // output: val_out - maximum digital gain
    SOC_SENSOR_GET_DIGITAL_GAIN_MAX,

    // return integration time latency
    // It means number of frames which is required for the sensor
    // between SOC_SENSOR_UPDATE_EXP is called and the actual value applied on the sensor side.
    // input: none
    // output: val_out - integration time latency
    SOC_SENSOR_GET_UPDATE_LATENCY,

    // return lines per second for the current mode
    // input: none
    // output: val_out - number of lines per second
    SOC_SENSOR_GET_LINES_PER_SECOND,

    // return current fps
    // input: sensor mode
    // output: val_out - current fps with 8 bits precesion
    SOC_SENSOR_GET_FPS,

    // return active image height
    // input: sensor mode
    // output: val_out - active height
    SOC_SENSOR_GET_ACTIVE_HEIGHT,

    // return active image width
    // input: sensor mode
    // output: val_out - active width
    SOC_SENSOR_GET_ACTIVE_WIDTH,

    // return current number of bits in raw
    // input: sensor mode
    // output: val_out - current number of bits in raw
   SOC_SENSOR_GET_SENSOR_BITS,

    //return current bayer pattern of setting
    //input: sensor mode
    //output: val_out - current bayer pattern
    SOC_SENSOR_GET_BAYER_PATTERN,

    // return current sensor name of setting
    // input: sensor mode
    // output: s_name - current sensor name
    SOC_SENSOR_GET_SENSOR_NAME,

    // return isp sequence of sensor
    // input: sensor mode
    // output: context_seq - current sensor's isp seq
    SOC_SENSOR_GET_CONTEXT_SEQ,

    SOC_SENSOR_SET_TEST_PATTERN,

    // return sensor ir cut set
    // input: ir cut mode
    // output: val_out - ir cut mode
    SOC_SENSOR_IR_CUT_SET,
};


#endif //__SOC_SENSOR_H__
