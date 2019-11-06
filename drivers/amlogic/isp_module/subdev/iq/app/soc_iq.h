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

#ifndef __SOC_IQ_H__
#define __SOC_IQ_H__

// This name is used by both V4L2 ISP device and
// V4L2 IQ subdevice to match the subdevice in the
// V4L2 async subdevice list.
#define V4L2_SOC_IQ_NAME "SocCalibrations"

// This is used as the main communication structure between
// V4L2 ISP Device and V4L2 IQ Subdevice
//
//
struct soc_iq_ioctl_args {
    union {
        // This struct is used to request information
        // about a LUT.
        // On this request the LookupTable lut
        // will be filled with data excluding the ptr.
        // ptr must be assigned to NULL.
        struct {
            uint32_t context; // must be 0
            void *sensor_arg; //sensor args instead of preset.
            uint32_t id;      // LUT ID value from acamera_command_api.h
            LookupTable lut;  // lut will be filled on return but ptr must be NULL
        } request_info;
        // This struct is used to request the actual LUT dat
        // The memory must be preallocated in advance and provided
        // by *ptr pointer.
        // The IQ V4L2 subdevice will copy LUT to the given memory as output
        struct {
            uint32_t context;   // must be always 0
            void *sensor_arg;   //sensor args instead of preset.
            uint32_t id;        // LUT ID value from acamera_command_api.h
            void *ptr;          // preallocated memory for the requested LUT data
            uint32_t data_size; // data size in bytes for ptr buffer
            uint32_t kernel;    // must be always 1
        } request_data;
    } ioctl;
};

// The enum defines possible commands ID for ioctl request from
// the V4L2 ISP device.
enum SocIQ_ioctl {
    // request the information about the LUT
    // it includes data type, number of elements
    // The given input structure will have type request_info.
    // Commonly used to calculate the size of the LUT before
    // allocating memory for the actual data.
    V4L2_SOC_IQ_IOCTL_REQUEST_INFO = 0,
    // request the whole LUT including the data.
    // The given input structure will have type request_data
    // Used to request the LUT data.
    V4L2_SOC_IQ_IOCTL_REQUEST_DATA,
};


#endif //__SOC_IQ_H__
