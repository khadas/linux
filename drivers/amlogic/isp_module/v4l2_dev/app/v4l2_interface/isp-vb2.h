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

#ifndef _ISP_VB2_H_
#define _ISP_VB2_H_

#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <linux/meson_ion.h>

#include "isp-v4l2-stream.h"
#include "isp-vb2-cmalloc.h"

/* VB2 control interfaces */
int isp_vb2_queue_init( struct vb2_queue *q, struct mutex *mlock, isp_v4l2_stream_t *pstream, struct device *dev );
void isp_vb2_queue_release( struct vb2_queue *q, isp_v4l2_stream_t *pstream );

#endif
