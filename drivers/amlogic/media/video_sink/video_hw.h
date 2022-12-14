/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_sink/video_hw_old.h
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

#ifndef VIDEO_HW_HH
#define VIDEO_HW_HH

#define FGRAIN_TBL_SIZE  (498 * 16)

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
void vpp_secure_cb(u32 arg);
#endif
void int_vpu_delay_work(void);

#endif
