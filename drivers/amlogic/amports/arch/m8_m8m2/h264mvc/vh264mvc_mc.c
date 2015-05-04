/*
 * drivers/amlogic/amports/arch/m8_m8m2/h264mvc/vh264mvc_mc.c
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

#define MicroCode vh264mvc_mc
#include "h264c_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_header_mc
#include "h264header_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_mmco_mc
#include "h264mmc_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_slice_mc
#include "h264slice_mvc_linux.h"

#define FOR_CPUS {MESON_CPU_MAJOR_ID_M8, MESON_CPU_MAJOR_ID_M8M2, 0}
#define FOR_VFORMAT VFORMAT_H264MVC

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMWARE(vh264mvc_mc);\
		DEF_FIRMWARE(vh264mvc_header_mc);\
		DEF_FIRMWARE(vh264mvc_mmco_mc);\
		DEF_FIRMWARE(vh264mvc_slice_mc);\
	} while (0)

INIT_DEF_FIRMWARE();
