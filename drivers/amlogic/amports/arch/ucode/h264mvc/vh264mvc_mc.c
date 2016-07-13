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

#include "../firmware_def.h"

#define MicroCode vh264mvc_mc
#include "h264c_mvc_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264mvc_mc
#include "gxm_h264c_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_header_mc
#include "h264header_mvc_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264mvc_header_mc
#include "gxm_h264header_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_mmco_mc
#include "h264mmc_mvc_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264mvc_mmco_mc
#include "gxm_h264mmc_mvc_linux.h"

#undef MicroCode
#define MicroCode vh264mvc_slice_mc
#include "h264slice_mvc_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264mvc_slice_mc
#include "gxm_h264slice_mvc_linux.h"

#define FOR_VFORMAT VFORMAT_H264MVC

#define DEF_FIRMEARE_FOR_GXM(n) \
		REGISTER_FIRMARE_PER_CPU(MESON_CPU_MAJOR_ID_GXM,\
		FOR_VFORMAT, n)

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMWARE(vh264mvc_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264mvc_mc);\
		DEF_FIRMWARE(vh264mvc_header_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264mvc_header_mc);\
		DEF_FIRMWARE(vh264mvc_mmco_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264mvc_mmco_mc);\
		DEF_FIRMWARE(vh264mvc_slice_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264mvc_slice_mc);\
	} while (0)

INIT_DEF_FIRMWARE();
