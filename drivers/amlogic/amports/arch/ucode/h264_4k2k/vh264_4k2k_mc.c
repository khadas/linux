/*
 * drivers/amlogic/amports/arch/m8_m8m2/h264_4k2k/vh264_4k2k_mc.c
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

#include "../firmware_def.h"

#define MicroCode vh264_4k2k_mc
#include "h264c_linux.h"

#undef MicroCode
#define MicroCode vh264_4k2k_mc_single
#include "h264c_linux_single.h"

#undef MicroCode
#define MicroCode vh264_4k2k_header_mc
#include "h264header_linux.h"

#undef MicroCode
#define MicroCode vh264_4k2k_header_mc_single
#include "h264header_linux_single.h"

#undef MicroCode
#define MicroCode vh264_4k2k_mmco_mc
#include "h264mmc_linux.h"

#undef MicroCode
#define MicroCode vh264_4k2k_mmco_mc_single
#include "h264mmc_linux_single.h"

#undef MicroCode
#define MicroCode vh264_4k2k_slice_mc
#include "h264slice_linux.h"

#undef MicroCode
#define MicroCode vh264_4k2k_slice_mc_single
#include "h264slice_linux_single.h"

#undef FOR_CPUS
#define FOR_CPUS {MESON_CPU_MAJOR_ID_M8M2, MESON_CPU_MAJOR_ID_GXBB, 0}
#define FOR_VFORMAT VFORMAT_H264_4K2K

#define DEF_FIRMEARE_FOR_M8(n) \
	REGISTER_FIRMARE_PER_CPU(MESON_CPU_MAJOR_ID_M8, FOR_VFORMAT, n)


#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMEARE_FOR_M8(vh264_4k2k_mc);\
		DEF_FIRMWARE(vh264_4k2k_mc_single);\
		DEF_FIRMEARE_FOR_M8(vh264_4k2k_header_mc);\
		DEF_FIRMWARE(vh264_4k2k_header_mc_single);\
		DEF_FIRMEARE_FOR_M8(vh264_4k2k_mmco_mc);\
		DEF_FIRMWARE(vh264_4k2k_mmco_mc_single);\
		DEF_FIRMEARE_FOR_M8(vh264_4k2k_slice_mc);\
		DEF_FIRMWARE(vh264_4k2k_slice_mc_single);\
	} while (0)

INIT_DEF_FIRMWARE();
