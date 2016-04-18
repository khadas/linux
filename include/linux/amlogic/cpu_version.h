/*
 * include/linux/amlogic/cpu_version.h
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


#ifndef __PLAT_MESON_CPU_H
#define __PLAT_MESON_CPU_H

#define MESON_CPU_TYPE_MESON1		0x10
#define MESON_CPU_TYPE_MESON2		0x20
#define MESON_CPU_TYPE_MESON3		0x30
#define MESON_CPU_TYPE_MESON6		0x60
#define MESON_CPU_TYPE_MESON6TV		0x70
#define MESON_CPU_TYPE_MESON6TVD	0x75
#define MESON_CPU_TYPE_MESON8		0x80
#define MESON_CPU_TYPE_MESON8B		0x8B
#define MESON_CPU_TYPE_MESONG9TV	0x90

/*
 *	Read back value for P_ASSIST_HW_REV
 *
 *	Please note: M8M2 readback value same as M8 (0x19)
 *			     We changed it to 0x1D in software,
 *			     Please ALWAYS use get_meson_cpu_version()
 *			     to get the version of Meson CPU
 */
#define MESON_CPU_MAJOR_ID_M6		0x16
#define MESON_CPU_MAJOR_ID_M6TV		0x17
#define MESON_CPU_MAJOR_ID_M6TVL	0x18
#define MESON_CPU_MAJOR_ID_M8		0x19
#define MESON_CPU_MAJOR_ID_MTVD		0x1A
#define MESON_CPU_MAJOR_ID_M8B		0x1B
#define MESON_CPU_MAJOR_ID_MG9TV	0x1C
#define MESON_CPU_MAJOR_ID_M8M2		0x1D
#define MESON_CPU_MAJOR_ID_GXBB		0x1F
#define MESON_CPU_MAJOR_ID_GXTVBB	0x20
#define MESON_CPU_MAJOR_ID_GXL		0x21
#define MESON_CPU_MAJOR_ID_GXM		0x22

#define MESON_CPU_VERSION_LVL_MAJOR	0
#define MESON_CPU_VERSION_LVL_MINOR	1
#define MESON_CPU_VERSION_LVL_PACK	2
#define MESON_CPU_VERSION_LVL_MISC	3
#define MESON_CPU_VERSION_LVL_MAX	MESON_CPU_VERSION_LVL_MISC
extern unsigned int system_serial_low0;
extern unsigned int system_serial_low1;
extern unsigned int system_serial_high0;
extern unsigned int system_serial_high1;

int  meson_cpu_version_init(void);
#ifdef CONFIG_AML_CPU_VERSION
int get_meson_cpu_version(int level);
#else
static inline int get_meson_cpu_version(int level)
{
	return -1;
}
#endif
static inline bool package_id_is(unsigned int id)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_PACK) & 0xF0) ==
		id;
}
static inline bool is_meson_m8_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_M8;
}

static inline bool is_meson_mtvd_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_MTVD;
}

static inline bool is_meson_m8b_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_M8B;
}

static inline bool is_meson_m8m2_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_M8M2;
}

static inline bool is_meson_g9tv_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_MG9TV;
}

static inline bool is_meson_gxbb_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_GXBB;
}

static inline bool is_meson_gxtvbb_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_GXTVBB;
}
static inline bool is_meson_gxbb_package_905(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_GXBB) &&
		((get_meson_cpu_version(MESON_CPU_VERSION_LVL_PACK) & 0xF0) !=
		0x20);
}
static inline bool is_meson_gxbb_package_905m(void)
{
	return (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_GXBB) &&
		((get_meson_cpu_version(MESON_CPU_VERSION_LVL_PACK) & 0xF0) ==
		0x20);
}
static inline bool is_meson_gxl_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_GXL;
}
static inline bool is_meson_gxl_package_905D(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x0);
}
static inline bool is_meson_gxl_package_905X(void)
{
	return is_meson_gxl_cpu() && package_id_is(0x80);
}
static inline bool is_meson_gxl_package_905L(void)
{
	return is_meson_gxl_cpu() && package_id_is(0xc0);
}
static inline bool is_meson_gxl_package_905M2(void)
{
	return is_meson_gxl_cpu() && package_id_is(0xe0);
}
static inline bool is_meson_gxm_cpu(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) ==
		MESON_CPU_MAJOR_ID_GXM;
}
static inline u32 get_cpu_type(void)
{
	return get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR);
}

#endif
