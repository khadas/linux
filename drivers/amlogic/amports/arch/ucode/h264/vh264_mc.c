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


#undef MicroCode
#define MicroCode gxm_vmh264_mc
#include "gxm_mh264c_linux.h"

#undef MicroCode
#define MicroCode gxm_vmh264_header_mc
#include "gxm_mh264header_linux.h"

#undef MicroCode
#define MicroCode gxm_vmh264_data_mc
#include "gxm_mh264data_linux.h"

#undef MicroCode
#define MicroCode gxm_vmh264_mmco_mc
#include "gxm_mh264mmc_linux.h"

#undef MicroCode
#define MicroCode gxm_vmh264_list_mc
#include "gxm_mh264list_linux.h"

#undef MicroCode
#define MicroCode gxm_vmh264_slice_mc
#include "gxm_mh264slice_linux.h"


const u32  vmh264_mc_mmu[] __initconst = {
	0x00000000, 0x00000000, 0x00000000, 0x0640c000, 0x0a610040,
	0x00000000, 0x0c000340, 0x00000000, 0x00000000, 0x0640c000,
	0x0a6100c0, 0x00000000, 0x0c000500, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x0c7ffc40, 0x00000000, 0x0640c811,
	0x06800413, 0x06800012, 0x06800010, 0x0c001280, 0x00000000,
	0x0c001440, 0x00000000, 0x0c001540, 0x00000000, 0x0800c002,
	0x0cc00000, 0x00000000, 0x00000000, 0x06400400, 0x09800680,
	0x06000400, 0x09c00680, 0x06000400, 0x0640c914, 0x0640ca15,
	0x06800016, 0x0c000740, 0x00000000, 0x04080514, 0x04080514,
	0x06800017, 0x06800018, 0x06463900, 0x09105000, 0x0b210200,
	0x00000000, 0x0c000740, 0x00000000, 0x0640c000, 0x0a610100,
	0x00000000, 0x0c7802c0, 0x00000000, 0x0ac06556, 0x00000000,
	0x0aab6018, 0x00000000, 0x0c7ffc40, 0x00000000, 0x0640c000,
	0x0a6fc100, 0x00000000, 0x0800c005, 0x0cc00000, 0x00000000,
	0x04000454, 0x06802013, 0x06800012, 0x068c0010, 0x0c0005c0,
	0x00000000, 0x0c000780, 0x00000000, 0x0cc00000, 0x00000000,
	0x06800000, 0x070005c1, 0x00000000, 0x040015d7, 0x070005c2,
	0x00000000, 0x040015d7, 0x09610042, 0x06063801, 0x04001000,
	0x0a6dc200, 0x00000000, 0x0487f5d7, 0x04010618, 0x04008596,
	0x0cc00000, 0x00000000, 0x06035111, 0x06035213, 0x04000012,
	0x06b00001, 0x02c01401, 0x02000040, 0x06035000, 0x0cc00000,
	0x00000000, 0x06435000, 0x0580f000, 0x0bef8040, 0x00000000,
	0x0cc00000, 0x00000000, 0x06035111, 0x06035213, 0x04000012,
	0x06a00001, 0x02000040, 0x06035000, 0x06435000, 0x0580f000,
	0x0bef8040, 0x00000000, 0x0cc00000, 0x00000000
};

#undef FOR_CPUS

#define FOR_GXBB_CPUS {MESON_CPU_MAJOR_ID_GXBB, 0}

#define FOR_GXTVBB_CPUS {MESON_CPU_MAJOR_ID_GXTVBB,\
		MESON_CPU_MAJOR_ID_GXL,\
		MESON_CPU_MAJOR_ID_GXLX, 0}

#define FOR_GXM_CPUS {MESON_CPU_MAJOR_ID_GXM,\
		MESON_CPU_MAJOR_ID_TXL,\
		MESON_CPU_MAJOR_ID_TXLX, 0}

#define FOR_GXTVBB_MULTI_CPUS {MESON_CPU_MAJOR_ID_GXBB,\
		MESON_CPU_MAJOR_ID_GXTVBB,\
		MESON_CPU_MAJOR_ID_GXL,\
		MESON_CPU_MAJOR_ID_GXLX, 0}

#define FOR_GXM_MULTI_CPUS {MESON_CPU_MAJOR_ID_GXM,\
		MESON_CPU_MAJOR_ID_TXL,\
		MESON_CPU_MAJOR_ID_TXLX, 0}


#define FOR_MMU_CPUS {MESON_CPU_MAJOR_ID_TXLX, 0}


#define F_VERSION "0.0.0.0"
#define FOR_VFORMAT VFORMAT_H264

#define DEF_FIRMEARE_FOR_GXBB(n) \
	DEF_FIRMWARE_FOR_CPUS_TYPE_VER(FOR_GXBB_CPUS, FOR_VFORMAT, n, F_VERSION)

#define DEF_FIRMEARE_FOR_GXTVBB(n) \
	DEF_FIRMWARE_FOR_CPUS_TYPE_VER(FOR_GXTVBB_CPUS,\
		FOR_VFORMAT, n, F_VERSION)

#define DEF_FIRMEARE_FOR_GXM(n) \
	DEF_FIRMWARE_FOR_CPUS_TYPE_VER(FOR_GXM_CPUS, FOR_VFORMAT, n, F_VERSION)

#define DEF_FIRMEARE_FOR_GXTVBB_MULTI(n) \
	DEF_FIRMWARE_FOR_CPUS_TYPE_VER(FOR_GXTVBB_MULTI_CPUS,\
	FOR_VFORMAT, n, F_VERSION)

#define DEF_FIRMEARE_FOR_GXM_MULTI(n) \
	DEF_FIRMWARE_FOR_CPUS_TYPE_VER(FOR_GXM_MULTI_CPUS,\
	FOR_VFORMAT, n, F_VERSION)


#define DEF_FIRMEARE_FOR_MMU(n) \
		DEF_FIRMWARE_FOR_CPUS_TYPE_VER(FOR_MMU_CPUS,\
		VFORMAT_HEVC, n, F_VERSION)

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMEARE_FOR_GXBB(vh264_mc);\
		DEF_FIRMEARE_FOR_GXTVBB(gxtvbb_vh264_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_header_mc);\
		DEF_FIRMEARE_FOR_GXTVBB(gxtvbb_vh264_header_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_header_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_data_mc);\
		DEF_FIRMEARE_FOR_GXTVBB(gxtvbb_vh264_data_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_data_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_mmco_mc);\
		DEF_FIRMEARE_FOR_GXTVBB(gxtvbb_vh264_mmco_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_mmco_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_list_mc);\
		DEF_FIRMEARE_FOR_GXTVBB(gxtvbb_vh264_list_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_list_mc);\
		DEF_FIRMEARE_FOR_GXBB(vh264_slice_mc);\
		DEF_FIRMEARE_FOR_GXTVBB(gxtvbb_vh264_slice_mc);\
		DEF_FIRMEARE_FOR_GXM(gxm_vh264_slice_mc);\
		DEF_FIRMEARE_FOR_GXTVBB_MULTI(vmh264_mc);\
		DEF_FIRMEARE_FOR_GXTVBB_MULTI(vmh264_header_mc);\
		DEF_FIRMEARE_FOR_GXTVBB_MULTI(vmh264_data_mc);\
		DEF_FIRMEARE_FOR_GXTVBB_MULTI(vmh264_mmco_mc);\
		DEF_FIRMEARE_FOR_GXTVBB_MULTI(vmh264_list_mc);\
		DEF_FIRMEARE_FOR_GXTVBB_MULTI(vmh264_slice_mc);\
		DEF_FIRMEARE_FOR_GXM_MULTI(gxm_vmh264_mc);\
		DEF_FIRMEARE_FOR_GXM_MULTI(gxm_vmh264_header_mc);\
		DEF_FIRMEARE_FOR_GXM_MULTI(gxm_vmh264_data_mc);\
		DEF_FIRMEARE_FOR_GXM_MULTI(gxm_vmh264_mmco_mc);\
		DEF_FIRMEARE_FOR_GXM_MULTI(gxm_vmh264_list_mc);\
		DEF_FIRMEARE_FOR_GXM_MULTI(gxm_vmh264_slice_mc);\
		DEF_FIRMEARE_FOR_MMU(vmh264_mc_mmu);\
	} while (0)


INIT_DEF_FIRMWARE();
