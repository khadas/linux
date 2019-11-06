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

#ifndef __SOC_LENS_H__
#define __SOC_LENS_H__

// This name is used by both V4L2 ISP device and
// V4L2 Lens subdevice to match the subdevice in the
// V4L2 async subdevice list.
#define V4L2_SOC_LENS_NAME "SocLens"

// This is used as the main communication structure between
// V4L2 ISP Device and V4L2 Lens Subdevice
// Parameters are used differently depending on the actual API command ID.
struct soc_lens_ioctl_args {
    uint32_t ctx_num;
    union {
        struct {
            uint32_t val_in;  // first input value for the API function (optional)
            uint32_t val_in2; // second input value for the API function (optional)
            uint32_t val_out; // output value returned by API function (optional)
        } general;
    } args;
};

// The enum declares the API commands ID which
// must be supported by V4L2 lens subdevice.
// This API ID will be used on each ioctl call from
// the V4L2 ISP Device.
enum SocLens_ioctl {
    // move the lens to the given position
    // input: val_in - target position for the lens
    // output: none
    SOC_LENS_MOVE = 0,

    // stop lens movement
    // input: none
    // output: none
    SOC_LENS_STOP,

    // get the current lens positon
    // input: none
    // output: val_out
    SOC_LENS_GET_POS,

    // return the lens status
    // input: none
    // output: val_out - 1 moving or 0 not moving
    SOC_LENS_IS_MOVING,

    // NOT SUPPORTED BY V4L2 ISP DEVICE
    // move zoom lens to the target position (if supported)
    // input: val_in - target position for the zoom lens
    // output: none
    SOC_LENS_MOVE_ZOOM,

    // NOT SUPPORTED BY V4L2 ISP DEVICE
    // return the zoom lens status
    // input: none
    // output: val_out - 1 zooming is in progress, 0 no active zooming
    SOC_LENS_IS_ZOOMING,

    // read the lens register
    // input: val_in - register address
    // output: val_out - register value
    SOC_LENS_READ_REG,

    // write the lens register
    // input: val_in - register address
    // input: val_in2 - register value
    // output: none
    SOC_LENS_WRITE_REG,

    // return lens type - static value
    // input: none
    // output: val_out - lens type
    SOC_LENS_GET_LENS_TYPE,

    // return minimum step size - static value
    // input: none
    // output: val_out - minimal step size
    SOC_LENS_GET_MIN_STEP,

    // NOT SUPPORTED BY V4L2 ISP DEVICE
    // next zoom lens position
    // input: none
    // output: val_out - next zoom lens position
    SOC_LENS_GET_NEXT_ZOOM,

    // NOT SUPPORTED BY V4L2 ISP DEVICE
    // current zoom value
    // input: none
    // output: val_out - current zoom value
    SOC_LENS_GET_CURR_ZOOM,

    // next lens position
    // input: none
    // output: val_out - next lens position
    SOC_LENS_GET_NEXT_POS
};


#endif //__SOC_LENS_H__
