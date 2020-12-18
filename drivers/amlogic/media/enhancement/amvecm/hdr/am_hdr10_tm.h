/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/set_hdr2_v0.h
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

#include "../amcsc.h"
#include "am_hdr10_tmo_fw.h"

#ifndef HDR10_TONE_MAPPING
#define HDR10_TONE_MAPPING

#define MAX12_BIT 12
#define OE_X 149
#define MAX32_BIT 32
#define MAX_BEIZER_ORDER 10
#define TM_GAIN_BIT 6
#define MAX_32 0xffffffff

extern unsigned int panell;
int hdr10_tm_dynamic_proc(struct vframe_master_display_colour_s *p);
#endif
