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

#ifndef __ACAMERA_AUTOCAP_API_H__
#define __ACAMERA_AUTOCAP_API_H__

#include "acamera_types.h"
#include "../app/v4l2_interface/isp-v4l2-stream.h"

#define AUTOCAP_MODE0 0xAA
#define AUTOCAP_MODE1 0x55

uint32_t autocap_get_mode(uint32_t ctx_id);
uint32_t autocap_pushbuf(int ctx_id, int d_type, tframe_t f_buff, isp_v4l2_stream_t *pstream);
uint32_t autocap_pullbuf(int ctx_id, int d_type, tframe_t f_buff);

#endif // __ACAMERA_AUTOCAP_API_H__