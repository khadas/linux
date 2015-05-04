/*
 * drivers/amlogic/amports/arch/m8_m8m2/h264/vh264_mc.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include "../../firmware.h"

#define MicroCode vh264_mc
#include "h264c_linux.h"

#undef MicroCode
#define MicroCode vh264_header_mc
#include "h264header_linux.h"

#undef MicroCode
#define MicroCode vh264_data_mc
#include "h264data_linux.h"

#undef MicroCode
#define MicroCode vh264_mmco_mc
#include "h264mmc_linux.h"

#undef MicroCode
#define MicroCode vh264_list_mc
#include "h264list_linux.h"

#undef MicroCode
#define MicroCode vh264_slice_mc
#include "h264slice_linux.h"

#define FOR_CPUS {MESON_CPU_MAJOR_ID_M8, MESON_CPU_MAJOR_ID_M8M2, 0}
#define FOR_VFORMAT VFORMAT_H264

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMWARE(vh264_mc);\
		DEF_FIRMWARE(vh264_header_mc);\
		DEF_FIRMWARE(vh264_data_mc);\
		DEF_FIRMWARE(vh264_mmco_mc);\
		DEF_FIRMWARE(vh264_list_mc);\
		DEF_FIRMWARE(vh264_slice_mc);\
	} while (0)

INIT_DEF_FIRMWARE();
