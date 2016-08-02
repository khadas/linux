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

#include "../firmware_def.h"

#define MicroCode vh264_mc
#include "h264c_linux.h"

#undef MicroCode
#define MicroCode gxtvbb_vh264_mc
#include "gxtvbb_h264c_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264_mc
#include "gxm_h264c_linux.h"

#undef MicroCode
#define MicroCode vh264_header_mc
#include "h264header_linux.h"

#undef MicroCode
#define MicroCode gxtvbb_vh264_header_mc
#include "gxtvbb_h264header_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264_header_mc
#include "gxm_h264header_linux.h"

#undef MicroCode
#define MicroCode vh264_data_mc
#include "h264data_linux.h"

#undef MicroCode
#define MicroCode gxtvbb_vh264_data_mc
#include "gxtvbb_h264data_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264_data_mc
#include "gxm_h264data_linux.h"

#undef MicroCode
#define MicroCode vh264_mmco_mc
#include "h264mmc_linux.h"

#undef MicroCode
#define MicroCode gxtvbb_vh264_mmco_mc
#include "gxtvbb_h264mmc_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264_mmco_mc
#include "gxm_h264mmc_linux.h"

#undef MicroCode
#define MicroCode vh264_list_mc
#include "h264list_linux.h"

#undef MicroCode
#define MicroCode gxtvbb_vh264_list_mc
#include "gxtvbb_h264list_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264_list_mc
#include "gxm_h264list_linux.h"

#undef MicroCode
#define MicroCode vh264_slice_mc
#include "h264slice_linux.h"

#undef MicroCode
#define MicroCode gxtvbb_vh264_slice_mc
#include "gxtvbb_h264slice_linux.h"

#undef MicroCode
#define MicroCode gxm_vh264_slice_mc
#include "gxm_h264slice_linux.h"

#undef MicroCode
#define MicroCode vmh264_mc
#include "mh264c_linux.h"

#undef MicroCode
#define MicroCode vmh264_header_mc
#include "mh264header_linux.h"

#undef MicroCode
#define MicroCode vmh264_data_mc
#include "mh264data_linux.h"

#undef MicroCode
#define MicroCode vmh264_mmco_mc
#include "mh264mmc_linux.h"

#undef MicroCode
#define MicroCode vmh264_list_mc
#include "mh264list_linux.h"

#undef MicroCode
#define MicroCode vmh264_slice_mc
#include "mh264slice_linux.h"

#undef FOR_CPUS
#define FOR_CPUS {MESON_CPU_MAJOR_ID_GXTVBB, MESON_CPU_MAJOR_ID_GXL,\
		MESON_CPU_MAJOR_ID_GXM, 0}

#define FOR_VFORMAT VFORMAT_H264

#define DEF_FIRMEARE_FOR_GXBB(n) \
		REGISTER_FIRMARE_PER_CPU(MESON_CPU_MAJOR_ID_GXBB,\
		FOR_VFORMAT, n)

#define DEF_FIRMEARE_FOR_GXM(n) \
		REGISTER_FIRMARE_PER_CPU(MESON_CPU_MAJOR_ID_GXM,\
		FOR_VFORMAT, n)

#define DEF_FIRMEARE_FOR_TXL(n) \
		REGISTER_FIRMARE_PER_CPU(MESON_CPU_MAJOR_ID_TXL,\
		FOR_VFORMAT, n)

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMEARE_FOR_GXBB(vh264_mc);\
		DEF_FIRMWARE(gxtvbb_vh264_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_mc);\
		DEF_FIRMEARE_FOR_TXL(gxm_vh264_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_header_mc);\
		DEF_FIRMWARE(gxtvbb_vh264_header_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_header_mc);\
		DEF_FIRMEARE_FOR_TXL(gxm_vh264_header_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_data_mc);\
		DEF_FIRMWARE(gxtvbb_vh264_data_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_data_mc);\
		DEF_FIRMEARE_FOR_TXL(gxm_vh264_data_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_mmco_mc);\
		DEF_FIRMWARE(gxtvbb_vh264_mmco_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_mmco_mc);\
		DEF_FIRMEARE_FOR_TXL(gxm_vh264_mmco_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_list_mc);\
		DEF_FIRMWARE(gxtvbb_vh264_list_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_list_mc);\
		DEF_FIRMEARE_FOR_TXL(gxm_vh264_list_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_slice_mc);\
		DEF_FIRMWARE(gxtvbb_vh264_slice_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_slice_mc);\
		DEF_FIRMEARE_FOR_TXL(gxm_vh264_slice_mc);\
		DEF_FIRMWARE(vmh264_mc);\
		DEF_FIRMWARE(vmh264_header_mc);\
		DEF_FIRMWARE(vmh264_data_mc);\
		DEF_FIRMWARE(vmh264_mmco_mc);\
		DEF_FIRMWARE(vmh264_list_mc);\
		DEF_FIRMWARE(vmh264_slice_mc);\
	} while (0)

INIT_DEF_FIRMWARE();
