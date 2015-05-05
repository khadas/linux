/*
 * drivers/amlogic/amports/arch/firmware.h
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

#ifndef __VIDEO_FIRMWARE_HEADER_
#define __VIDEO_FIRMWARE_HEADER_
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/amports/vformat.h>

struct video_firmware_s {

	int cpu_type;

	enum vformat_e type;

	const char *name;

	int size;

	int ref_cnt;

	const char *version;

	struct video_firmware_s *next;

	const char ucode[1];	/*malloced more for ucode. */
};
int register_video_firamre_per_cpu(int cputype, enum vformat_e type,
		const char *name, const char *code, int size, const char *ver);
int register_video_firamre(int cpus[], enum vformat_e type, const char *name,
		const char *code, int size, const char *ver);
int get_decoder_firmware_data(enum vformat_e type, const char *file_name,
		char *buf, int size);

int show_all_buildin_firmwares(void);

#define F_VERSION "0.0.0.0"

#define REGISTER_FIRMARE_PER_CPU_VER(cpu, type, name, ver)\
		register_video_firamre_per_cpu(cpu, type,\
		#name, (const char *)name, sizeof(name), ver)

#define REGISTER_FIRMARE_PER_CPU(cpu, type, name)\
		REGISTER_FIRMARE_PER_CPU_VER(cpu, type, name, F_VERSION);

#define REGISTER_FIRMARE_IN(cpus, type, name, ver)\
		register_video_firamre(cpus, type,\
		#name, (const char *)name, sizeof(name), ver)

#define DEF_FIRMWARE_FOR_CPUS_TYPE_VER(cpus, type, name, ver)\
	do {\
		int t_cpus[] = cpus;\
		REGISTER_FIRMARE_IN(t_cpus, type, name, ver);\
	} while (0)

#define DEF_FIRMWARE_FOR_CPUS_TYPE(cpus, type, name)\
		DEF_FIRMWARE_FOR_CPUS_TYPE_VER(cpus, type, name, F_VERSION)

#define DEF_FIRMWARE_FOR_CPUS_VER(cpus, name, ver)\
		DEF_FIRMWARE_FOR_CPUS_TYPE_VER(cpus, FOR_VFORMAT, name, ver);

#define DEF_FIRMWARE_FOR_CPUS(cpus, name)\
		DEF_FIRMWARE_FOR_CPUS_TYPE(cpus, FOR_VFORMAT, name);

#define DEF_FIRMWARE_VER(name, ver)\
		DEF_FIRMWARE_FOR_CPUS_TYPE_VER(FOR_CPUS, FOR_VFORMAT, name, ver)

#define DEF_FIRMWARE(name)\
		DEF_FIRMWARE_FOR_CPUS_TYPE_VER(FOR_CPUS, \
		FOR_VFORMAT, name, F_VERSION)

#define INIT_DEF_FIRMWARE()\
static int __init init_ucode_per_cpu(void)\
	{\
		REG_FIRMWARE_ALL();\
		return 0;\
	}	\
module_init(init_ucode_per_cpu);



/*
sample:

#define PER_CPUS {MESON_CPU_MAJOR_ID_M8,MESON_CPU_MAJOR_ID_M8M2,0}
#define FOR_VFORMAT VFORMAT_H264

#define REG_FIRMWARE_ALL()\
		DEF_FIRMWARE(vh264_mc);\
		DEF_FIRMWARE(vh264_header_mc);\
		DEF_FIRMWARE(vh264_data_mc);\
		DEF_FIRMWARE(vh264_mmco_mc);\
		DEF_FIRMWARE(vh264_list_mc);\
		DEF_FIRMWARE(vh264_slice_mc);\
		DEF_FIRMWARE_VER(vh264_slice_mc,"1.0");\

INIT_DEF_FIRMWARE();
*/
#endif
