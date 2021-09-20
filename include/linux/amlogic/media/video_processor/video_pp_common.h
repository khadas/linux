/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/video_processor/video_pp_common.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef VIDEO_PP_COMMON_H
#define VIDEO_PP_COMMON_H

#include <linux/amlogic/media/vfm/vframe.h>

int get_ge2d_input_format(struct vframe_s *vf);

#endif /* VIDEO_PP_COMMON_H */

