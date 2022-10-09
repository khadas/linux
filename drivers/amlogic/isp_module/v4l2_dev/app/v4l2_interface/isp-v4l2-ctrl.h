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

#ifndef _ISP_V4L2_CTRL_H_
#define _ISP_V4L2_CTRL_H_

#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#define std_hdl_to_isp_ctrl( hdl ) container_of( hdl, isp_v4l2_ctrl_t, ctrl_hdl_std_ctrl )
#define cst_hdl_to_isp_ctrl( hdl ) container_of( hdl, isp_v4l2_ctrl_t, ctrl_hdl_cst_ctrl )

typedef struct _isp_v4l2_ctrl {
    /* Fields need to be filled by owner */
    struct v4l2_device *v4l2_dev;
    struct video_device *video_dev;

    /* Fields will be filled by isp_v4l2_ctrl module */
    uint32_t ctx_id;
    struct v4l2_ctrl_handler ctrl_hdl_std_ctrl; /* STD ctrl */
    struct v4l2_ctrl_handler ctrl_hdl_cst_ctrl; /* CST ctrl */
} isp_v4l2_ctrl_t;

/* external interface to isp-v4l2 module */
int isp_v4l2_ctrl_init( uint32_t ctx_id, isp_v4l2_ctrl_t *ctrl );
void isp_v4l2_ctrl_deinit( isp_v4l2_ctrl_t *dev );

#endif
